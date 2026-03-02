$env:BDS = 'C:\Program Files (x86)\Embarcadero\Studio\37.0'
$env:BDSINCLUDE = 'C:\Program Files (x86)\Embarcadero\Studio\37.0\include'
$env:BDSCOMMONDIR = 'C:\Users\Public\Documents\Embarcadero\Studio\37.0'
$env:FrameworkDir = 'C:\Windows\Microsoft.NET\Framework\v4.0.30319'
$env:PATH = 'C:\Windows\Microsoft.NET\Framework\v4.0.30319;C:\Program Files (x86)\Embarcadero\Studio\37.0\bin;C:\Program Files (x86)\Embarcadero\Studio\37.0\bin64;' + $env:PATH

Set-Location 'C:\RADProjects\MCPHub'
& 'C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe' MCPHub.cbproj /p:Config=Release /p:Platform=Win64x /t:Build /v:minimal 2>&1
