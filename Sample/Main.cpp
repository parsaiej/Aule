#include <Aule/Aule.h>

#include <stdexcept>
#include <iostream>

int main(int argc, char** argv)
{
    auto RenderLambda = [&](Aule::FrameContext& context) {};

    Aule::Params params = {};
    {
        params.windowName     = "Aule Sample";
        params.windowWidth    = 1280u;
        params.windowHeight   = 720u;
        params.renderCallback = RenderLambda;
    }

    try
    {
        Aule::Go(params);
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Fatal: " << e.what() << std::endl;
    }

    return 0;
}
