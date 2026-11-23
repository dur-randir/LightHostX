//
//  AppProperties.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef AppProperties_hpp
#define AppProperties_hpp

#include "JuceHeader.h"

class AppProperties {
private:
    inline static std::unique_ptr<ApplicationProperties> appProperties;
    inline static bool initialized = false;

public:
    static PropertiesFile* get() {
        if (!initialized) {
            initialized = true;

            PropertiesFile::Options options;
            options.applicationName     = "LightHostX";
            options.filenameSuffix      = "settings";
            options.osxLibrarySubFolder = "Application Support/LightHostX";

            appProperties = std::make_unique<ApplicationProperties>();
            appProperties->setStorageParameters(options);
        }
        
        return appProperties->getUserSettings();
    };
    
    static void saveIfNeeded() {
        if (initialized) {
            appProperties->saveIfNeeded();
        }
    }
    
    static void destroy() {
        if (initialized) {
            appProperties->saveIfNeeded();
        }
        appProperties.reset();
    }
};

#endif /* AppProperties_hpp */
