#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR 4
#define CLIENT_VERSION_MINOR 0
#define CLIENT_VERSION_REVISION 3
#define CLIENT_VERSION_BUILD 0

// Set to true for release, false for prerelease or test build
#define CLIENT_VERSION_IS_RELEASE true

// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif  // CLIENTVERSION_H
