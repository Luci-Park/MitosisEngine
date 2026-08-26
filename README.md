
This is a CMAKE project that supports both windows and linux. Therefore it is recommended to use VSCode instead of Visual Studio.

# Adding Modules and Files
This engine is aiming for modularity, therefore each part of the engine is it's own static library. To make things easier VSCode tasks has been added.

## Adding Modules
1. `Ctrl + Shift + P`
2. `Tasks: Run Task`
3. Select `New Module`
4. Give it a name

## Adding Files
1. `Ctrl + Shift + P`
2. `Tasks: Run Task`
3. Select `New File`
4. Type the target module
5. Give it a name
6. Select class(.h + .cpp) or header(.h)