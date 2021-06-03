Get-ChildItem -Path .\src -Include *.hpp, *.cpp -Recurse | 
ForEach-Object {
    Write-Output $_.FullName
    &clang-format -i -style=file $_.FullName
}
