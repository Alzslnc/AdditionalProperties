using ArxBridge;
using BaseFunction;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;

namespace ArxBridgeUI.UI
{
    internal class SettingsViewModel : BaseClass
    {
        private ObservableCollection<ProviderUiWrapper> _uiProviders;
        public ObservableCollection<ProviderUiWrapper> UiProviders
        {
            get => _uiProviders;
            set => SetData(ref _uiProviders, value);
        }

        public ICommand SaveCommand { get; }
        public ICommand CloseCommand { get; }

        private System.Windows.Window _windowInstance;

        public SettingsViewModel()
        {
            // Оборачиваем C++ модули в C# обертки для построения дерева
            var wrappers = BridgeManager.Providers
                .Select(p => new ProviderUiWrapper(p))
                .ToList();

            UiProviders = new ObservableCollection<ProviderUiWrapper>(wrappers);

            SaveCommand = new RelayCommand(OnSave);
            CloseCommand = new RelayCommand(OnClose);
        }

        internal static void Launch()
        {
            var view = new SettingsView();
            var viewModel = new SettingsViewModel();

            view.DataContext = viewModel;
            viewModel._windowInstance = view;

            // Показываем модальное окно в AutoCAD
            Autodesk.AutoCAD.ApplicationServices.Application.ShowModalWindow(view);
        }

        private void OnSave(object obj)
        {
            // Вызываем твой метод обновления. 
            // Так как обертки меняли IsEnabled напрямую в объектах C++, 
            // менеджеру достаточно просто зафиксировать изменения (или перечитать конфигурацию)
            BridgeManager.UpdateProviders();

            _windowInstance?.Close();
        }

        private void OnClose(object obj)
        {
            // Закрываем без сохранения (изменения флагов в памяти откатятся при следующем открытии окна, 
            // так как данные перезагрузятся заново из BridgeManager.Providers)
            _windowInstance?.Close();
        }
    }
}
