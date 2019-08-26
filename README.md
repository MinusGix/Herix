## Intial Notes
This is a hex editor library that is meant to have a GUI (or TUI, when I refer to GUI I just mean some form of displaying it to the user) written that uses it, it is *not* the GUI itself. This lets it not have the gui handling code so built into itself, which is good and bad.  
  
The main class `Herix` is what you will instantiate for your editor. It stores the loaded data and it's edits separately, and the edits only store what is needed.  