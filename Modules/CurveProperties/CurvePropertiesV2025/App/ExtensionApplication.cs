using Autodesk.AutoCAD.Runtime;
using BaseFunction;
using CurveProperties.Ribbon;
using System;
using ArxBridge;

namespace CurveProperties.App
{
    public class ExtensionApplication : IExtensionApplication
    {
        public void Initialize()
        {
            new StartEvents().Initialize(Commands.CommandClasses);
            CountMenus.CreateCountMenus();
            Autodesk.AutoCAD.ApplicationServices.Core.Application.Idle += Wait;
        }
        public void Terminate()
        {
        }
        private static void Wait(object sender, EventArgs e)
        {
            if (Autodesk.AutoCAD.ApplicationServices.Application.DocumentManager != null)
            {
                Autodesk.AutoCAD.ApplicationServices.Core.Application.Idle -= Wait;
                BridgeManager.RegisterProvider(new CurvePropertiesDataClass());
            }
        }
    }
}
