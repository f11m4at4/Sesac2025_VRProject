// @lint-ignore-every LICENSELINT
// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusXRToolStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FOculusToolStyle::StyleInstance = nullptr;

void FOculusToolStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FOculusToolStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FOculusToolStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("OculusToolStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(Style->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(Style->RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef<FSlateStyleSet> FOculusToolStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("OculusToolStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("OculusXR")->GetBaseDir() / TEXT("Resources"));

	Style->Set("OculusTool.MenuButton", new IMAGE_BRUSH(TEXT("ButtonIcon_80x"), Icon40x40));
	Style->Set("OculusTool.OpenPluginWindow", new IMAGE_BRUSH(TEXT("ButtonIcon_80x"), Icon40x40));
	Style->Set("OculusTool.MetaLogo",
		new FSlateVectorImageBrush(Style->RootToContentDir("MetaLogo", TEXT(".svg")),
			FVector2D(32.0f, 32.0f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FOculusToolStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FOculusToolStyle::Get()
{
	return *StyleInstance;
}
