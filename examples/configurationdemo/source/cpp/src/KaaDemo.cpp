/*
 * Copyright 2014-2015 CyberVision, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <memory>
#include <thread>

#include <kaa/Kaa.hpp>
#include <kaa/IKaaClient.hpp>

#include <kaa/profile/DefaultProfileContainer.hpp>

#include <kaa/configuration/storage/IConfigurationPersistenceManager.hpp>
#include <kaa/configuration/manager/IConfigurationReceiver.hpp>
#include <kaa/configuration/storage/FileConfigurationStorage.hpp>

#include <kaa/logging/Log.hpp>

#include <stdio.h>

using namespace kaa;
using std::cout;
using std::endl;

class UserConfigurationReceiver : public IConfigurationReceiver {
public:
    void displayConfiguration(const KaaRootConfiguration &configuration)
    {
        if (!configuration.AddressList.is_null()) {
            cout<<"Configuration body:\n";
            auto links = configuration.AddressList.get_array();
            for(auto& e : links) {
                cout<<e.label<<" - "<<e.url<<endl;
            }
        }
    }
    virtual void onConfigurationUpdated(const KaaRootConfiguration &configuration)
    {
        cout<<"Configuration was updated\n";
        displayConfiguration(configuration);
    }
};


int main()
{
    Kaa::init();
    cout<<"Configuration demo started"<<endl;
    IKaaClient& kaaClient = Kaa::getKaaClient();
    // Set up default profile container
    kaaClient.setProfileContainer(std::make_shared<DefaultProfileContainer>());
    kaaClient.updateProfile();
    // Set up a configuration subunit
    IConfigurationStoragePtr storage(std::make_shared<FileConfigurationStorage>("saved_config.cfg"));
    kaaClient.setConfigurationStorage(storage);
    UserConfigurationReceiver receiver;
    kaaClient.addConfigurationListener(receiver);

    Kaa::start();

    for (int i = 0; i < 100; ++i)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    Kaa::stop();
    cout<<"Configuration demo stopped"<<endl;
    return 0;
}
