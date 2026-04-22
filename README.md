# KeePass Keylogging BOF 

1. Detect KeePass process by name
2. Install keyboard hook on KeePass thread
3. Monitor window title to detect when the database opens ("Open Database - xxxx" -> "Database.kdbx")
4. Stop keyboard hook and print keystrokes
5. Wakeup

# Acknowledgements

- https://maldevacademy.com/new/modules/67
- https://github.com/DarksBlackSk/Keylogger-BOF

