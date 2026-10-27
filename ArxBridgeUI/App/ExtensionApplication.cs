using ArxBridgeUI.Ribbon;
using Autodesk.AutoCAD.Runtime;
using BaseFunction;

namespace ArxBridgeUI.App
{
    public class ExtensionApplication : IExtensionApplication
    {
        public void Initialize()
        {
            new StartEvents().Initialize(Commands.CommandClasses);
            CountMenus.CreateCountMenus();
        }
        public void Terminate()
        {
        }
    }
}
