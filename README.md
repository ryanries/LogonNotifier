LogonNotifier 1.0 by Joseph Ryan Ries
A test service to demonstrate registering for user logon session change notifications.

Usage:
  LogonNotifier -install
  LogonNotifier -uninstall

This program only runs when installed as a Windows service.

The service is written in C, and compiled in Visual Studio 2015. I used /Wall to enable all warnings.

The service runs as Local Service, which is quite safe compared to Local System.

It outputs a logfile to %SystemRoot%\ServiceProfiles\LocalService\AppData\Local\LogonNotifier.log.