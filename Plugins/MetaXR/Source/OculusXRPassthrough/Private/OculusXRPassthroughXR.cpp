// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRPassthroughXR.h"

#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "IOpenXRHMDModule.h"
#include "Materials/Material.h"
#include "OculusXRHMD/Private/OculusXRResourceHolder.h"
#include "OculusXRHMD_CustomPresent.h"
#include "OculusXRHMDRuntimeSettings.h"
#include "OculusXRPassthroughXRFunctions.h"
#include "OculusXRPassthroughModule.h"
#include "OculusXRPassthroughEventHandling.h"
#include "OpenXRHMD.h"
#include "OpenXRHMD_Swapchain.h"
#include "StereoRendering.h"
#include "RenderGraphBuilder.h"

#include "XRThreadUtils.h"

#define LOCTEXT_NAMESPACE "OculusXRPassthrough"

namespace XRPassthrough
{
	TWeakPtr<FPassthroughXR> FPassthroughXR::GetInstance()
	{
		return FOculusXRPassthroughModule::Get().GetPassthroughExtensionPlugin();
	}

	FPassthroughXR::FPassthroughXR()
		: InvAlphaTexture(nullptr)
		, ColorSwapchain(nullptr)
		, ColorSwapChainTexture(nullptr)
		, ProjectionLayerAlphaBlend{}
		, Layers_RenderThread{}
		, bPassthroughInitialized(false)
		, PassthroughInstance{ XR_NULL_HANDLE }
		, Settings(nullptr)
	{
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		Settings = MakeShareable(new FSettings());
	}

	FPassthroughXR::~FPassthroughXR()
	{
	}

	void FPassthroughXR::RegisterAsOpenXRExtension()
	{
#if defined(WITH_OCULUS_BRANCH)
		// Feature not enabled on Marketplace build. Currently only for the meta fork
		RegisterOpenXRExtensionModularFeature();
#endif
	}

	bool FPassthroughXR::GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		OutExtensions.Add(XR_FB_PASSTHROUGH_EXTENSION_NAME);
		OutExtensions.Add(XR_META_PASSTHROUGH_LAYER_RESUMED_EVENT_EXTENSION_NAME);
		return true;
	}

	bool FPassthroughXR::GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		OutExtensions.Add(XR_FB_COMPOSITION_LAYER_ALPHA_BLEND_EXTENSION_NAME);
		OutExtensions.Add(XR_FB_TRIANGLE_MESH_EXTENSION_NAME);
		OutExtensions.Add(XR_META_PASSTHROUGH_COLOR_LUT_EXTENSION_NAME);
		return true;
	}

	const void* FPassthroughXR::OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext)
	{
		if (InModule != nullptr)
		{
			Settings->bExtLayerAlphaBlendAvailable = InModule->IsExtensionEnabled(XR_FB_COMPOSITION_LAYER_ALPHA_BLEND_EXTENSION_NAME);
			Settings->bExtPassthroughAvailable = InModule->IsExtensionEnabled(XR_FB_PASSTHROUGH_EXTENSION_NAME);
			Settings->bExtTriangleMeshAvailable = InModule->IsExtensionEnabled(XR_FB_TRIANGLE_MESH_EXTENSION_NAME);
			Settings->bExtColorLutAvailable = InModule->IsExtensionEnabled(XR_META_PASSTHROUGH_COLOR_LUT_EXTENSION_NAME);
			Settings->bExtLayerResumedEventAvailable = InModule->IsExtensionEnabled(XR_META_PASSTHROUGH_LAYER_RESUMED_EVENT_EXTENSION_NAME);
		}
		return InNext;
	}

	const void* FPassthroughXR::OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
	{
		InitOpenXRFunctions(InInstance);

		Settings->PokeAHoleMaterial = Cast<UMaterial>(FSoftObjectPath(TEXT("/OculusXR/Materials/PokeAHoleMaterial")).TryLoad());

		OpenXRHMD = (FOpenXRHMD*)GEngine->XRSystem.Get();

		const UOculusXRHMDRuntimeSettings* HMDSettings = GetDefault<UOculusXRHMDRuntimeSettings>();
		Settings->bPassthroughEnabled = HMDSettings->bInsightPassthroughEnabled;

		return InNext;
	}

	void FPassthroughXR::PostCreateSession(XrSession InSession)
	{
		if (Settings->bPassthroughEnabled)
		{
			ExecuteOnRenderThread([this, InSession](FRHICommandListImmediate& RHICmdList) {
				InitializePassthrough(InSession);
			});
		}
	}

	void FPassthroughXR::OnDestroySession(XrSession InSession)
	{
		// Release resources
		ExecuteOnRenderThread([this, InSession]() {
			Layers_RenderThread.Reset();
			InvAlphaTexture.SafeRelease();

			DeferredDeletion.HandleLayerDeferredDeletionQueue_RenderThread(true);

			ShutdownPassthrough(InSession);
		});

		OpenXRHMD = nullptr;
	}

	void* FPassthroughXR::OnWaitFrame(XrSession InSession, void* InNext)
	{
		Update_GameThread(InSession);
		return InNext;
	}

	bool FPassthroughXR::IsPassthroughEnabled(void) const
	{
		return bPassthroughInitialized;
	}

	void FPassthroughXR::InvertTextureAlpha_RenderThread(FRHICommandList& RHICmdList, FRHITexture* Texture, FRHITexture* TempTexture, const FIntRect& ViewportRect)
	{
		{
			FRHITexture* SrcTexture = Texture;
			FRHITexture* DstTexture = TempTexture;
			const FIntRect SrcRect(ViewportRect);
			const FIntRect DstRect(0, 0, ViewportRect.Size().X, ViewportRect.Size().Y);

			const bool bAlphaPremultiply = false;
			const bool bNoAlphaWrite = false;
			const bool bInvertSrcY = false;
			const bool sRGBSource = false;
			const bool bInvertAlpha = true;
			const auto FeatureLevel = GEngine ? GEngine->GetDefaultWorldFeatureLevel() : GMaxRHIFeatureLevel;
			const bool bUsingVulkan = RHIGetInterfaceType() == ERHIInterfaceType::Vulkan;
			OculusXRHMD::FCustomPresent::CopyTexture_RenderThread(RHICmdList.GetAsImmediate(), RendererModule, DstTexture, SrcTexture, FeatureLevel, bUsingVulkan,
				DstRect, SrcRect, bAlphaPremultiply, bNoAlphaWrite, bInvertSrcY, sRGBSource, bInvertAlpha);
		}

		{
			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(ViewportRect.Size().X, ViewportRect.Size().Y, 1);
			CopyInfo.SourcePosition = FIntVector::ZeroValue;
			CopyInfo.DestPosition = FIntVector(ViewportRect.Min.X, ViewportRect.Min.Y, 0);
			CopyInfo.SourceSliceIndex = 0;
			CopyInfo.DestSliceIndex = 0;

			if (Texture->GetDesc().IsTextureArray() && TempTexture->GetDesc().IsTextureArray())
			{
				CopyInfo.NumSlices = FMath::Min(Texture->GetDesc().ArraySize, TempTexture->GetDesc().ArraySize);
			}

			FRHITexture* SrcTexture = TempTexture;
			FRHITexture* DstTexture = Texture;
			RHICmdList.Transition(FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			RHICmdList.CopyTexture(SrcTexture, DstTexture, CopyInfo);
			RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(SrcTexture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		}
	}

	FPassthroughLayerPtr FPassthroughXR::CreateStereoLayerFromDesc(const IStereoLayers::FLayerDesc& LayerDesc) const
	{
		FPassthroughLayerPtr Layer = nullptr;

		if (FPassthroughLayer::IsPassthoughLayerDesc(LayerDesc))
		{
			check(PassthroughInstance != XR_NULL_HANDLE);
			Layer = MakeShareable(new FPassthroughLayer(PassthroughInstance, GetInstance()));
		}
		return Layer;
	}

	void FPassthroughXR::OnSetupLayers_RenderThread(XrSession InSession, const TArray<uint32_t>& LayerIds)
	{
		check(IsInRenderingThread());

#ifdef WITH_OCULUS_BRANCH
		ColorSwapchain = OpenXRHMD->GetColorSwapchain_RenderThread();
#endif

#if PLATFORM_WINDOWS
		if (ColorSwapchain && InvAlphaTexture == nullptr)
		{
#if UE_VERSION_OLDER_THAN(5, 5, 0)
			const auto CVarPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
			const bool bPropagateAlpha = EAlphaChannelMode::FromInt(CVarPropagateAlpha->GetValueOnRenderThread()) == EAlphaChannelMode::AllowThroughTonemapper;
#else
			const auto CVarPropagateAlpha = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
			const bool bPropagateAlpha = CVarPropagateAlpha->GetBool();
#endif
			if (bPropagateAlpha)
			{
				const FRHITextureDesc TextureDesc = ColorSwapchain->GetTexture()->GetDesc();
				uint32 SizeX = TextureDesc.GetSize().X;
				uint32 SizeY = TextureDesc.GetSize().Y;
				EPixelFormat ColorFormat = TextureDesc.Format;
				uint32 NumMips = TextureDesc.NumMips;
				uint32 NumSamples = TextureDesc.NumSamples;

				FClearValueBinding ColorTextureBinding = FClearValueBinding::Black;

				const ETextureCreateFlags InvTextureCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
				FRHITextureCreateDesc InvTextureDesc{};
				if (ColorSwapchain->GetTexture2DArray() != nullptr)
				{
					InvTextureDesc = FRHITextureCreateDesc::Create2DArray(TEXT("InvAlphaTexture"))
										 .SetArraySize(2)
										 .SetExtent(SizeX, SizeY)
										 .SetFormat(ColorFormat)
										 .SetNumMips(NumMips)
										 .SetNumSamples(NumSamples)
										 .SetFlags(InvTextureCreateFlags | TexCreate_TargetArraySlicesIndependently)
										 .SetClearValue(ColorTextureBinding);
				}
				else
				{
					InvTextureDesc = FRHITextureCreateDesc::Create2D(TEXT("InvAlphaTexture"))
										 .SetExtent(SizeX, SizeY)
										 .SetFormat(ColorFormat)
										 .SetNumMips(NumMips)
										 .SetNumSamples(NumSamples)
										 .SetFlags(InvTextureCreateFlags)
										 .SetClearValue(ColorTextureBinding);
				}
				InvAlphaTexture = RHICreateTexture(InvTextureDesc);
			}
		}
#endif
	}

	void FPassthroughXR::UpdateCompositionLayers(XrSession InSession, TArray<XrCompositionLayerBaseHeaderType*>& Headers)
	{
		check(IsInRenderingThread() || IsInRHIThread());

		TArray<FPassthroughLayerPtr> SortedLayers = Layers_RenderThread;
		SortedLayers.Sort(FLayerDesc_ComparePriority());

		// Headers array already contains (at least) one layer which is the eye's layer.
		// Underlay/SupportDetph layers need to be inserted before that layer, ordered by priority.
		int EyeLayerId = 0;
		for (const FPassthroughLayerPtr& Layer : SortedLayers)
		{
			if (Layer->IsBackgroundLayer() || Layer->PassthroughSupportsDepth())
			{
				XrCompositionLayerBaseHeaderType* CompositionLayerHeader = Layer->GetXrCompositionLayerHeader();
				if (CompositionLayerHeader != nullptr)
				{
					Headers.Insert(CompositionLayerHeader, EyeLayerId++);
				}
			}
			else if (Layer->IsOverlayLayer())
			{
				XrCompositionLayerBaseHeaderType* CompositionLayerHeader = Layer->GetXrCompositionLayerHeader();
				if (CompositionLayerHeader != nullptr)
				{
					Headers.Add(CompositionLayerHeader);
				}
			}
		}
	}

#ifdef WITH_OCULUS_BRANCH
	bool FPassthroughXR::OnEndGameFrame(FWorldContext& WorldContext)
	{
		FXRTrackingSystemBase* TS = static_cast<FXRTrackingSystemBase*>(GEngine->XRSystem.Get());
		TrackingToWorld = TS->GetTrackingToWorldTransform();
		WorldToMetersScale = TS->GetWorldToMetersScale();
		return true;
	}
#endif

	void FPassthroughXR::OnBeginRendering_GameThread(XrSession InSession)
	{
		// Send game thread layers to render thread ones
		TArray<FPassthroughLayerPtr> XLayers;
		XLayers.Empty(LayerMap.Num());

		for (auto& Pair : LayerMap)
		{
			XLayers.Emplace(Pair.Value->Clone());
		}

		XLayers.Sort(FPassthroughLayerPtr_CompareId());

		ENQUEUE_RENDER_COMMAND(TransferFrameStateToRenderingThread)
		([this, TrackingToWorld = TrackingToWorld, WorldToMetersScale = WorldToMetersScale, XLayers, InSession](FRHICommandListImmediate& RHICmdList) mutable {
			TrackingToWorld_RenderThread = TrackingToWorld;
			WorldToMetersScale_RenderThread = WorldToMetersScale;

			int32 XLayerIndex = 0;
			int32 LayerIndex_RenderThread = 0;
			TArray<FPassthroughLayerPtr> ValidLayers;

			// Scan for changes
			while (XLayerIndex < XLayers.Num() && LayerIndex_RenderThread < Layers_RenderThread.Num())
			{
				uint32 LayerIdA = XLayers[XLayerIndex]->GetDesc().GetLayerId();
				uint32 LayerIdB = Layers_RenderThread[LayerIndex_RenderThread]->GetDesc().GetLayerId();

				if (LayerIdA < LayerIdB) // If a layer was inserted in the middle of existing ones
				{
					if (XLayers[XLayerIndex]->Initialize_RenderThread(InSession))
					{
						ValidLayers.Add(XLayers[XLayerIndex]);
					}
					XLayerIndex++;
				}
				else if (LayerIdA > LayerIdB) // If a layer was removed in the middle of existing ones
				{
					DeferredDeletion.AddOpenXRLayerToDeferredDeletionQueue(Layers_RenderThread[LayerIndex_RenderThread++]);
				}
				else // This layer is not new nor removed
				{
					if (XLayers[XLayerIndex]->Initialize_RenderThread(InSession, Layers_RenderThread[LayerIndex_RenderThread].Get()))
					{
						LayerIndex_RenderThread++;
						ValidLayers.Add(XLayers[XLayerIndex]);
					}
					XLayerIndex++;
				}
			}

			// Create missing layers
			while (XLayerIndex < XLayers.Num())
			{
				if (XLayers[XLayerIndex]->Initialize_RenderThread(InSession))
				{
					ValidLayers.Add(XLayers[XLayerIndex]);
				}
				XLayerIndex++;
			}

			// Delete remaining layers
			while (LayerIndex_RenderThread < Layers_RenderThread.Num())
			{
				DeferredDeletion.AddOpenXRLayerToDeferredDeletionQueue(Layers_RenderThread[LayerIndex_RenderThread++]);
			}

			Layers_RenderThread = ValidLayers;

			DeferredDeletion.HandleLayerDeferredDeletionQueue_RenderThread();
		});
	}

#ifdef WITH_OCULUS_BRANCH
	void FPassthroughXR::OnBeginRenderingLate_RenderThread(XrSession InSession, FRHICommandListImmediate& RHICmdList)
	{
		ColorSwapchain = OpenXRHMD->GetColorSwapchain_RenderThread();

		if (ColorSwapchain && InvAlphaTexture)
		{
			ColorSwapChainTexture = ColorSwapchain->GetTexture();
		}
	}

	void FPassthroughXR::FinishRenderFrame_RenderThread(FRDGBuilder& GraphBuilder)
	{
		check(IsInRenderingThread());

		if (ColorSwapChainTexture && InvAlphaTexture)
		{
			FRDGEventName PassName = RDG_EVENT_NAME("FPassthroughXR_InvertTextureAlpha");
			GraphBuilder.AddPass(MoveTemp(PassName), ERDGPassFlags::None,
				[this, SwapchainTexture = ColorSwapChainTexture](FRHICommandListImmediate& RHICmdList) {
					const FRHITextureDesc TextureDesc = SwapchainTexture->GetDesc();
					FIntRect TextureRect = FIntRect(0, 0, TextureDesc.GetSize().X, TextureDesc.GetSize().Y);

					InvertTextureAlpha_RenderThread(RHICmdList, SwapchainTexture, InvAlphaTexture, TextureRect);
				});
		}

		XrSpace Space = OpenXRHMD->GetTrackingSpace();
		XrTime DisplayTime = OpenXRHMD->GetDisplayTime();

		FRDGEventName PassName = RDG_EVENT_NAME("FPassthroughXR_UpdatePassthroughLayers");
		GraphBuilder.AddPass(MoveTemp(PassName), ERDGPassFlags::None,
			[this, Space, DisplayTime](FRHICommandListImmediate& RHICmdList) {
				for (const FPassthroughLayerPtr& Layer : Layers_RenderThread)
				{
					Layer->UpdatePassthrough_RenderThread(RHICmdList,
						Space,
						DisplayTime,
						WorldToMetersScale_RenderThread,
						TrackingToWorld_RenderThread);
				}
			});
	}
#endif

	void FPassthroughXR::OnEvent(XrSession InSession, const XrEventDataBaseHeader* InHeader)
	{
		switch (InHeader->type)
		{
			case XR_TYPE_EVENT_DATA_PASSTHROUGH_LAYER_RESUMED_META:
				if (Settings->bExtLayerResumedEventAvailable)
				{
					const XrEventDataPassthroughLayerResumedMETA* LayerResumedEvent =
						reinterpret_cast<const XrEventDataPassthroughLayerResumedMETA*>(InHeader);

					for (auto& Pair : LayerMap)
					{
						if (Pair.Value->GetLayerHandle() == LayerResumedEvent->layer)
						{
							UE_LOG(LogOculusXRPassthrough, Log, TEXT("FOculusXRPassthroughEventHandling - Passthrough Layer #%d resumed"), Pair.Value->GetDesc().GetLayerId());

							// Send event
							OculusXRPassthrough::FOculusXRPassthroughEventDelegates::OculusPassthroughLayerResumed.Broadcast(Pair.Value->GetDesc().GetLayerId());
							break;
						}
					}
				}
				break;
		}
	}

	const void* FPassthroughXR::OnEndProjectionLayer(XrSession InSession, int32 InLayerIndex, const void* InNext, XrCompositionLayerFlags& OutFlags)
	{
		check(IsInRenderingThread() || IsInRHIThread());

		bool bHasBackgroundLayer = false;
		for (const FPassthroughLayerPtr& Layer : Layers_RenderThread)
		{
			if (Layer->IsBackgroundLayer() || Layer->PassthroughSupportsDepth())
			{
				bHasBackgroundLayer = true;
				break;
			}
		}

		if (bHasBackgroundLayer)
		{
			OutFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

			if (Settings->bExtLayerAlphaBlendAvailable)
			{
				InNext = &ProjectionLayerAlphaBlend;
				ProjectionLayerAlphaBlend.type = XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB;
				ProjectionLayerAlphaBlend.srcFactorColor = XR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA_FB;
				ProjectionLayerAlphaBlend.srcFactorAlpha = XR_BLEND_FACTOR_ONE_FB;
				ProjectionLayerAlphaBlend.dstFactorColor = XR_BLEND_FACTOR_SRC_ALPHA_FB;
				ProjectionLayerAlphaBlend.dstFactorAlpha = XR_BLEND_FACTOR_ZERO_FB;
				ProjectionLayerAlphaBlend.next = nullptr;
			}
			else
			{
				// XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is required for the eye layer to be correctly blended
				// when XR_FB_composition_layer_alpha_blend extension is not available (e.g. Link)
				OutFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
			}
		}

		return InNext;
	}

#ifdef WITH_OCULUS_BRANCH
	void FPassthroughXR::OnCreateLayer(const IStereoLayers::FLayerDesc& InLayerDesc, uint32 LayerId)
	{
		OculusXRHMD::CheckInGameThread();

		if (FPassthroughLayerPtr Layer = CreateStereoLayerFromDesc(InLayerDesc))
		{
			Layer->SetDesc(InLayerDesc);

			LayerMap.Add(LayerId, Layer);
		}
	}

	void FPassthroughXR::OnDestroyLayer(uint32 LayerId)
	{
		OculusXRHMD::CheckInGameThread();

		FPassthroughLayerPtr* LayerFound = LayerMap.Find(LayerId);
		if (LayerFound)
		{
			(*LayerFound)->DestroyLayer();
		}
		LayerMap.Remove(LayerId);
	}

	void FPassthroughXR::OnSetLayerDesc(uint32 LayerId, const IStereoLayers::FLayerDesc& InLayerDesc)
	{
		OculusXRHMD::CheckInGameThread();

		FPassthroughLayerPtr* LayerFound = LayerMap.Find(LayerId);

		if (LayerFound)
		{
			(*LayerFound)->SetDesc(InLayerDesc);
		}
	}
#endif

	void FPassthroughXR::InitializePassthrough(XrSession InSession)
	{
		if (bPassthroughInitialized)
			return;

		bPassthroughInitialized = true;

		check(IsInRenderingThread());

		const XrPassthroughCreateInfoFB PassthroughCreateInfo = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };

		XrResult CreatePassthroughResult = xrCreatePassthroughFB(InSession, &PassthroughCreateInfo, &PassthroughInstance);
		if (!XR_SUCCEEDED(CreatePassthroughResult))
		{
			UE_LOG(LogOculusXRPassthrough, Error, TEXT("xrCreatePassthroughFB failed, error : %i"), CreatePassthroughResult);
			return;
		}

		XrResult PassthroughStartResult = xrPassthroughStartFB(PassthroughInstance);
		if (!XR_SUCCEEDED(PassthroughStartResult))
		{
			UE_LOG(LogOculusXRPassthrough, Error, TEXT("xrPassthroughStartFB failed, error : %i"), PassthroughStartResult);
			return;
		}
	}

	void FPassthroughXR::ShutdownPassthrough(XrSession InSession)
	{
		if (!bPassthroughInitialized)
			return;

		bPassthroughInitialized = false;

		check(IsInRenderingThread());

		if (PassthroughInstance != XR_NULL_HANDLE)
		{
			XrResult Result = xrDestroyPassthroughFB(PassthroughInstance);
			if (!XR_SUCCEEDED(Result))
			{
				UE_LOG(LogOculusXRPassthrough, Error, TEXT("xrDestroyPassthroughFB failed, error : %i"), Result);
			}
			PassthroughInstance = nullptr;
		}
	}

	void FPassthroughXR::Update_GameThread(XrSession InSession)
	{
		check(IsInGameThread());

		check(Settings != nullptr);
		const bool bPassthroughEnabled = Settings->bPassthroughEnabled;

		ExecuteOnRenderThread_DoNotWait([this, InSession, bPassthroughEnabled](FRHICommandListImmediate& RHICmdList) {
			if (bPassthroughEnabled && !bPassthroughInitialized)
			{
				InitializePassthrough(InSession);
			}

			if (!bPassthroughEnabled && bPassthroughInitialized)
			{
				ShutdownPassthrough(InSession);
			}
		});
	}

} // namespace XRPassthrough

#undef LOCTEXT_NAMESPACE
