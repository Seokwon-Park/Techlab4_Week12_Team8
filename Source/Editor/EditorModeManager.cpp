#include "EnginePCH.h"
#include "Editor/EditorModeManager.h"

FEditorModeTools& GLevelEditorModeTools()
{
	static FEditorModeTools LevelEditorModeTools;
	return LevelEditorModeTools;
}
