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
    CGRect keyboardFrame = CGRectZero; // in coordinate dello schermo

    juce::ChangeBroadcaster& broadcaster()
    {
        static juce::ChangeBroadcaster b;
        return b;
    }

    void keyboardChanged()
    {
        broadcaster().sendChangeMessage();
    }

    void installObservers()
    {
        if (observersInstalled)
            return;

        observersInstalled = true;

        NSNotificationCenter* centre = [NSNotificationCenter defaultCenter];

        [centre addObserverForName: UIKeyboardDidShowNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification* n)
        {
            keyboardVisible = true;
            keyboardFrame = [[n.userInfo objectForKey: UIKeyboardFrameEndUserInfoKey] CGRectValue];
            keyboardChanged();
        }];

        [centre addObserverForName: UIKeyboardDidHideNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification*)
        {
            keyboardVisible = false;
            keyboardFrame = CGRectZero;
            keyboardChanged();
        }];

        // Tastiera flottante/agganciata o divisa: il frame cambia senza
        // Show/Hide. WillChangeFrame (non Did) cosi' il layout si muove
        // insieme all'animazione della tastiera, non dopo.
        [centre addObserverForName: UIKeyboardWillChangeFrameNotification object: nil queue: [NSOperationQueue mainQueue]
                        usingBlock: ^(NSNotification* n)
        {
            const CGRect frame = [[n.userInfo objectForKey: UIKeyboardFrameEndUserInfoKey] CGRectValue];
            const CGRect screen = [UIScreen mainScreen].bounds;
            keyboardVisible = CGRectIntersectsRect (frame, screen) && frame.size.height > 0;
            keyboardFrame = keyboardVisible ? frame : CGRectZero;
            keyboardChanged();
        }];
    }
}

namespace IOSKeyboard
{
    void initialise()
    {
        installObservers();
    }

    bool isVisible()
    {
        installObservers();
        return keyboardVisible;
    }

    juce::Rectangle<int> getFrameOnScreen()
    {
        installObservers();

        if (! keyboardVisible)
            return {};

        // Tastiera flottante (iPad): molto piu' stretta dello schermo, non
        // "copre" in modo prevedibile -> nessuna riduzione del layout.
        const CGRect screen = [UIScreen mainScreen].bounds;

        if (keyboardFrame.size.width < screen.size.width * 0.6)
            return {};

        return { (int) keyboardFrame.origin.x, (int) keyboardFrame.origin.y,
                 (int) keyboardFrame.size.width, (int) keyboardFrame.size.height };
    }

    juce::ChangeBroadcaster& getBroadcaster()
    {
        return broadcaster();
    }

    bool clipboardHasText()
    {
        return [[UIPasteboard generalPasteboard] hasStrings]; // non legge: nessun avviso di sistema
    }
}

#else

namespace IOSKeyboard
{
    void initialise() {}
    bool isVisible() { return true; }
    juce::Rectangle<int> getFrameOnScreen() { return {}; }
    juce::ChangeBroadcaster& getBroadcaster() { static juce::ChangeBroadcaster b; return b; }
    bool clipboardHasText() { return juce::SystemClipboard::getTextFromClipboard().isNotEmpty(); }
}

#endif
