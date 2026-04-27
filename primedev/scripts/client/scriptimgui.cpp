#include "squirrel/squirrel.h"
#include "../../imgui/imgui_ws_test.h"

ADD_SQFUNC("void", NSDrawImGuiFromScript, "string message", "", ScriptContext::UI)
{
	const SQChar* message = g_pSquirrel[context]->getstring(sqvm, 1);

	RenderScriptThing(message);

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", ImGui_BeginFrame, "", "", ScriptContext::UI)
{
	BeginScriptFrame();

	// TEMP - Menu for basic rendering tests
	ImGui::Begin("HELLO WORLD", 0, ImGuiWindowFlags_AlwaysAutoResize);

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", ImGui_EndFrame, "", "", ScriptContext::UI)
{
	// TEMP - Menu for basic rendering tests
	ImGui::End();

	EndScriptFrame();

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", ImGui_Text, "string contents", "", ScriptContext::UI)
{
	const SQChar* message = g_pSquirrel[context]->getstring(sqvm, 1);
	ImGui::Text("%s", message);

	return SQRESULT_NULL;
}
