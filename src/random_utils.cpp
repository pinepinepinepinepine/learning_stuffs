#include "includes.hpp"

// This is nothing more than just a handy thingy to know in order to check extensions/libs... do whatever!
void some_handy_printer( vk::raii::Context* context, std::vector<const char*>* req_layers, std::vector<const char*>* req_extensions  )
{
    // For a list of all the supported extensions this app has available, use this.
    auto extensions = (*context).enumerateInstanceExtensionProperties();

    // For a list of all the supported layers this app uses has available, use this.
    auto layers = (*context).enumerateInstanceLayerProperties();

    std::cout << "extensions this app has available\n";
    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    std::cout << "extensions actually required\n";
    for (const auto& requiredExtension : *req_extensions ) {
        std::cout << '\t' << requiredExtension << '\n';
    }

    std::cout << "layers this app has available\n";
    for (const auto& layer : layers ) {
        std::cout << '\t' << layer.layerName << '\n';
    }

    std::cout << "layers actually required\n";
    for (const auto& requiredLayer : *req_layers ) {
        std::cout << '\t' << requiredLayer << '\n';
    }
    std::cout << "\n\n";
}

const auto start_time = std::chrono::steady_clock::now();
double get_current_time()
{
    return std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - start_time ).count();
}

