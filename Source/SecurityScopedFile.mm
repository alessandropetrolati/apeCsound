// Foundation PRIMA di JUCE (vedi il commento in SecurityScopedFile.h sul
// conflitto con MacTypes.h "Point"): MacTypes.h viene cosi' letto prima
// che JuceHeader.h apra "using namespace juce".
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
 #import <Foundation/Foundation.h>
#endif

#include "SecurityScopedFile.h"

#if JUCE_IOS
namespace juce
{
    // Dichiarate (friend di juce::URL) e definite in juce_URL.cpp, solo iOS.
    void* getURLBookmark (URL&);
    void  setURLBookmark (URL&, void*);
}

namespace
{
    // Conversioni locali: juceStringToNS/nsStringToJuce sono helper interni
    // di JUCE (juce_ObjCHelpers_mac.h), non esportati dai moduli.
    NSString* toNS (const juce::String& s)   { return [NSString stringWithUTF8String: s.toRawUTF8()]; }
    juce::String fromNS (NSString* s)        { return s != nil ? juce::String::fromUTF8 ([s UTF8String]) : juce::String(); }
}
#endif

namespace SecurityScopedFile
{
    juce::MemoryBlock bookmarkFromChooserURL (juce::URL& url)
    {
       #if JUCE_IOS
        if (auto* data = (NSData*) juce::getURLBookmark (url))
            return juce::MemoryBlock ([data bytes], (size_t) [data length]);
       #else
        juce::ignoreUnused (url);
       #endif
        return {};
    }

    juce::URL makeURLWithBookmark (const juce::File& file, const juce::MemoryBlock& bookmark)
    {
        juce::URL url (file);

       #if JUCE_IOS
        if (bookmark.getSize() > 0)
        {
            // setURLBookmark prende possesso dell'NSData (lo rilascia
            // URL::Bookmark::~Bookmark): retain esplicito.
            NSData* data = [[NSData alloc] initWithBytes: bookmark.getData() length: bookmark.getSize()];
            juce::setURLBookmark (url, (void*) data);
        }
       #endif

        return url;
    }

    juce::MemoryBlock makeBookmark (const juce::File& file)
    {
       #if JUCE_IOS
        NSURL* nsurl = [NSURL fileURLWithPath: toNS (file.getFullPathName())];
        NSError* error = nil;
        NSData* data = [nsurl bookmarkDataWithOptions: 0
                       includingResourceValuesForKeys: nil
                                        relativeToURL: nil
                                                error: &error];

        if (error == nil && data != nil)
            return juce::MemoryBlock ([data bytes], (size_t) [data length]);
       #else
        juce::ignoreUnused (file);
       #endif
        return {};
    }

    ScopedAccess::ScopedAccess (const juce::MemoryBlock& bookmark, const juce::File& fallback)
        : resolvedFile (fallback)
    {
       #if JUCE_IOS
        if (bookmark.getSize() == 0)
            return;

        NSData* data = [NSData dataWithBytes: bookmark.getData() length: bookmark.getSize()];
        BOOL isStale = NO;
        NSError* error = nil;

        NSURL* url = [NSURL URLByResolvingBookmarkData: data
                                               options: 0
                                         relativeToURL: nil
                                   bookmarkDataIsStale: &isStale
                                                 error: &error];

        if (error != nil || url == nil)
            return;

        stale = isStale;

        if ([url startAccessingSecurityScopedResource])
        {
            active = true;
            nsurl = (void*) [url retain];
            resolvedFile = juce::File (fromNS ([url path]));
        }
       #else
        juce::ignoreUnused (bookmark);
       #endif
    }

    ScopedAccess::~ScopedAccess()
    {
       #if JUCE_IOS
        if (nsurl != nullptr)
        {
            NSURL* url = (NSURL*) nsurl;
            [url stopAccessingSecurityScopedResource];
            [url release];
        }
       #endif
    }
}
