// UIKit PRIMA di JuceHeader (incluso da IOSKeyboard.h): vedi il commento
// su "Point"/MacTypes.h in SecurityScopedFile.h.
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
 #import <UIKit/UIKit.h>
#endif

#include "IOSKeyboard.h"

#if TARGET_OS_IPHONE

namespace
{
    // Senza ARC (come gli altri .mm del progetto). Gli osservatori vivono
    // per tutta la durata del processo: non vengono mai rimossi.
    bool keyboardVisible = false;
    bool observersInstalled = false;

    void installObservers()
    {
        if (observersInstalled)
            return;

        observersInstalled = true;

        NSNotificationCenter* centre = [NSNotificationCenter defaultCenter];

        [centre addObserverForName: UIKeyboardDidShowNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification*) { keyboardVisible = true; }];

        [centre addObserverForName: UIKeyboardDidHideNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification*) { keyboardVisible = false; }];

        // Tastiera flottante/agganciata: il frame cambia senza Show/Hide.
        [centre addObserverForName: UIKeyboardDidChangeFrameNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification* n)
        {
            const CGRect frame = [[n.userInfo objectForKey: UIKeyboardFrameEndUserInfoKey] CGRectValue];
            const CGRect screen = [UIScreen mainScreen].bounds;
            keyboardVisible = CGRectIntersectsRect (frame, screen) && frame.size.height > 0;
        }];
    }
}

namespace IOSKeyboard
{
    bool isVisible()
    {
        installObservers();
        return keyboardVisible;
    }

    bool clipboardHasText()
    {
        return [[UIPasteboard generalPasteboard] hasStrings]; // non legge: nessun avviso di sistema
    }
}

#else

namespace IOSKeyboard
{
    bool isVisible() { return true; }
    bool clipboardHasText() { return juce::SystemClipboard::getTextFromClipboard().isNotEmpty(); }
}

#endif
