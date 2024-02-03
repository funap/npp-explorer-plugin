# Explorer Plugin for Notepad++ x64
This is a modified version of the [Explorer] plugin for [Notepad++].
You could easy browse through files and edit sources in Notepad++.

## Difference between the original plugin
- Notepad++ 64-bit compatible
- More configurable actions to the plugin menu.
- Quick Open Feature.  
  If you press <kbd>Ctrl+P</kbd>, the Fuzzy Finder will pop up.  
  This will let you quickly search for any file in current path by typing parts of the file name.
- Full tree Feature.  
  Ability to display files in a tree view like Light Explorer.
- And, minor bug fixes. See also the [Releases] for more information.

## Download
https://github.com/funap/npp-explorer-plugin/releases

## Screenshot
- Explorer Panel & Plugin Menu  
  ![Screenshot]

- Explorer Panel (use Full tree)
  ![Screenshot2]

- Quick Open  
  ![QuickOpen]

## Installation

### Notepad++ 7.6.3 and above
Drop the `Explorer\Explorer.dll` into the `%ProgramFiles%\Notepad++\plugins\` folder. The `Explorer` subfolder has to be kept.
i.e.  
`C:\Program Files\Notepad++\plugins\Explorer\Explorer.dll`

### older versions
#### Notepad++ 7.6.1
Drop the `Explorer\Explorer.dll` into the `%ProgramData%\Notepad++\plugins\` folder. The `Explorer` subfolder has to be kept.
i.e.  
`C:\ProgramData\Notepad++\plugins\Explorer\Explorer.dll`

#### Notepad++ 7.6
Drop the `Explorer\Explorer.dll` into the `%LocalAppData%\Notepad++\plugins\` folder. The `Explorer` subfolder has to be kept.
i.e.  
`C:\Users\[USERNAME]\AppData\Local\Notepad++\plugins\Explorer\Explorer.dll`

#### 7.5.9 or lower
Just copy the `Explorer.dll` to your `Notepad++\plugins\` directory.

## License
This project is licensed under the terms of the GNU GPL v2.0 license

[Explorer]: http://sourceforge.net/projects/npp-plugins/files/Explorer/
[Notepad++]: http://notepad-plus-plus.org/
[Screenshot]: https://raw.githubusercontent.com/funap/npp-explorer-plugin/master/.github/screenshot.png "Screenshot"
[Screenshot2]: https://raw.githubusercontent.com/funap/npp-explorer-plugin/master/.github/screenshot2.png "Screenshot2"
[QuickOpen]: https://raw.githubusercontent.com/funap/npp-explorer-plugin/master/.github/quickopen.gif "Screenshot"
[releases]: https://github.com/funap/npp-explorer-plugin/releases