using Autodesk.AutoCAD.Runtime;
using System;

namespace CurveProperties.App
{
    public class ExtensionApplication : IExtensionApplication
    {
        public void Initialize()
        {
            Autodesk.AutoCAD.ApplicationServices.Core.Application.Idle += Wait;
        }
        public void Terminate()
        {
        }
        /// <summary>
        /// запускаем регистрацию нашего класса после того как кад загрузится
        /// что бы мост к этому моменту был гарантировано запущен
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private static void Wait(object sender, EventArgs e)
        {
            if (Autodesk.AutoCAD.ApplicationServices.Application.DocumentManager != null)
            {
                Autodesk.AutoCAD.ApplicationServices.Core.Application.Idle -= Wait;
                //Регистрируем наш класс в мосте
                ArxBridge.BridgeManager.RegisterProvider(new CurvePropertiesDataClass());
            }
        }
    }
}
