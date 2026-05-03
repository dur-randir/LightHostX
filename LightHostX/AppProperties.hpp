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
    static std::unique_ptr<ApplicationProperties> appProperties;
    inline static bool initialized = false;

public:
    static PropertiesFile* get() {
        if (!initialized) {
            initialized = true;

            PropertiesFile::Options options;
            options.applicationName     = "LightHostX";
            options.filenameSuffix      = "settings";

            appProperties->setStorageParameters(options);
        }
        
        return appProperties->getUserSettings();
    };
    
    static void saveIfNeeded() {
        if (initialized) {
            appProperties->saveIfNeeded();
        }
    }
};

#endif /* AppProperties_hpp */
