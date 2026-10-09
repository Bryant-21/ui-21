Scriptname MCM Native Hidden

bool Function IsInstalled() global native
int Function GetVersionCode() global native
Function RefreshMenu() global native

int Function GetModSettingInt(string asModName, string asSetting) global native
bool Function GetModSettingBool(string asModName, string asSetting) global native
float Function GetModSettingFloat(string asModName, string asSetting) global native
string Function GetModSettingString(string asModName, string asSetting) global native

Function SetModSettingInt(string asModName, string asSettingName, int aiValue) global native
Function SetModSettingBool(string asModName, string asSettingName, bool abValue) global native
Function SetModSettingFloat(string asModName, string asSettingName, float afValue) global native
Function SetModSettingString(string asModName, string asSettingName, string asValue) global native
