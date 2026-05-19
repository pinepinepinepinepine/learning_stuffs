#include "../headers/renderer.hpp"
#include "../headers/runtime.hpp"


int main()
{
    RenderApplication app;
    app.setup();
    RunTimeApplication run( &app );


    run.run();







    app.cleanup();
    std::cout << "\nSuccessfully Executed!\n";
    return 0;
}

// TODO: It is likely a good idea to have structs inherent from the Device struct
    // this is because these objects are all owned by a Device, also would be WAY nicer because we don't have to constantly pass device as a param.
    // we COULD also make it a member (pointer to a device), but then it's like... added memory. Future problem!

// TODO: we also need to setup a colour image, and depth image. (createDepthResources + createColorResources()) (for whenever we begin recording for the pipeline)