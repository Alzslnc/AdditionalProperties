using System;
using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.ApplicationServices;

namespace CurveProperties
{
    public class ExtensionApplication : IExtensionApplication
    {
        public void Initialize()
        {
            // Если чертеж уже открыт (например, модуль загрузили посреди работы)
            if (Application.DocumentManager.MdiActiveDocument != null)
            {
                RunRegistration();
            }
            else
            {
                // Ждем, пока пользователь создаст или откроет первый чертеж
                Application.DocumentManager.DocumentCreated += DocumentManager_DocumentCreated;
            }
           
        }

        private void DocumentManager_DocumentCreated(object sender, DocumentCollectionEventArgs e)
        {
            // Чертеж открылся! Нативный COM-менеджер свойств AutoCAD на 100% готов!
            RunRegistration();

            // Отписываемся, чтобы не регистрировать свойства повторно при открытии новых вкладок
            Application.DocumentManager.DocumentCreated -= DocumentManager_DocumentCreated;
        }

        private void RunRegistration()
        {
            try
            {
                // Передаем наш класс. Вызов уйдет через Object^, склеится внутри С++
                // и динамически зарегистрирует "Угол", "Длину" и т.д. прямо из этого модуля!
                ArxBridge.BridgeManager.RegisterProvider((object)new CurvePropertiesDataClass());
            }
            catch (System.Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Ошибка регистрации: {ex.Message}");
            }
        }

        public void Terminate()
        {
            Application.DocumentManager.DocumentCreated -= DocumentManager_DocumentCreated;
        }
    }
}
