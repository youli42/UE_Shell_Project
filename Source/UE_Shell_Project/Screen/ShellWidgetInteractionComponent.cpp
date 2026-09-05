#include "ShellWidgetInteractionComponent.h"

#include "Framework/Application/SlateApplication.h"

void UShellWidgetInteractionComponent::SyncModifierKeys()
{
	ModifierKeys = FSlateApplication::Get().GetModifierKeys();
}
