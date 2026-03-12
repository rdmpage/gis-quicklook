/*
 * GISTypes/main.c
 *
 * Minimal stub whose sole purpose is to give Launch Services a valid
 * app bundle so it will register the UTImportedTypeDeclarations in
 * Info.plist.  Without a registered UTI, macOS assigns a dynamic UTI
 * to GIS files and the QuickLook generator is never invoked.
 *
 * This executable is never meant to be launched by the user.
 */
int main(void) { return 0; }
