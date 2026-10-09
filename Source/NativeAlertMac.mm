// NESSUN header JUCE qui - vedi NativeAlertMac.h. Solo Cocoa/UIKit, const
// char* in ingresso e una callback C in uscita.

#if defined(__APPLE__)

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
 #import <UIKit/UIKit.h>
#else
 #import <Cocoa/Cocoa.h>
#endif

extern "C" void showNativeAlertRaw (const char* title, const char* message,
                                    const char* button1Text, const char* button2Text, const char* button3Text,
                                    void (*callback) (int result, void* context), void* context,
                                    void* nativeView)
{
   #if TARGET_OS_IPHONE
    UIAlertController* alert = [UIAlertController alertControllerWithTitle: [NSString stringWithUTF8String: title]
                                                                   message: [NSString stringWithUTF8String: message]
                                                            preferredStyle: UIAlertControllerStyleAlert];

    const char* titles[3] = { button1Text, button2Text, button3Text };

    for (int i = 0; i < 3; ++i)
    {
        if (titles[i] == nullptr)
            continue;

        const int result = i + 1;
        NSString* t = [NSString stringWithUTF8String: titles[i]];
        const bool isCancel = [t caseInsensitiveCompare: @"Cancel"] == NSOrderedSame;

        UIAlertAction* action = [UIAlertAction actionWithTitle: t
                                                         style: isCancel ? UIAlertActionStyleCancel : UIAlertActionStyleDefault
                                                       handler: ^(UIAlertAction*) { callback (result, context); }];
        [alert addAction: action];

        if (i == 0)
            alert.preferredAction = action; // = Invio su tastiera esterna
    }

    // View controller su cui presentare: si risale dalla UIView del plugin
    // lungo la catena dei responder (funziona anche nell'estensione AUv3,
    // dove UIApplication non e' disponibile). Fallback: finestra chiave.
    UIViewController* root = nil;

    if (nativeView != nullptr)
    {
        UIResponder* responder = (UIView*) nativeView;

        while (responder != nil && ! [responder isKindOfClass: [UIViewController class]])
            responder = [responder nextResponder];

        root = (UIViewController*) responder;
    }

    if (root == nil)
    {
        Class appClass = NSClassFromString (@"UIApplication");
        id app = appClass != nil ? [appClass performSelector: @selector (sharedApplication)] : nil;

        for (UIWindow* w in [app windows])
            if (w.isKeyWindow) { root = w.rootViewController; break; }

        if (root == nil)
            root = [[app windows] firstObject].rootViewController;
    }

    while (root.presentedViewController != nil)
        root = root.presentedViewController;

    if (root == nil)
    {
        callback (0, context);
        return;
    }

    [root presentViewController: alert animated: YES completion: nil];
   #else
    NSAlert* alert = [[NSAlert alloc] init];

    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = [NSString stringWithUTF8String: title];
    alert.informativeText = [NSString stringWithUTF8String: message];

    // Il PRIMO bottone aggiunto e' quello che NSAlert posiziona piu' a
    // destra (l'azione di default, scattabile anche con Invio).
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button1Text]];
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button2Text]];

    if (button3Text != nullptr)
        [alert addButtonWithTitle: [NSString stringWithUTF8String: button3Text]];

    (void) nativeView;
    const NSModalResponse response = [alert runModal];

    int result = 0;
    if (response == NSAlertFirstButtonReturn)       result = 1;
    else if (response == NSAlertSecondButtonReturn) result = 2;
    else if (response == NSAlertThirdButtonReturn)  result = 3;

    callback (result, context);
   #endif
}

#endif // defined(__APPLE__)
