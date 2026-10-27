using ArxBridge;
using BaseFunction;
using System.Collections.ObjectModel;
using System.Linq;

namespace ArxBridgeUI.UI
{
    // Обертка для конкретного свойства (PropertyMeta)
    public class PropertyUiWrapper : BaseClass
    {
        public PropertyMeta UnderlyingMeta { get; }
        public string PropertyName { get; }

        public bool IsEnabled
        {
            get => UnderlyingMeta.IsEnabled;
            set
            {
                bool current = UnderlyingMeta.IsEnabled;
                if (SetData(ref current, value))
                {
                    UnderlyingMeta.IsEnabled = value;
                }
            }
        }

        public PropertyUiWrapper(string name, PropertyMeta meta)
        {
            PropertyName = name;
            UnderlyingMeta = meta;
        }
    }

    // Обертка для модуля (SelectedDataClass)
    public class ProviderUiWrapper : BaseClass
    {
        public SelectedDataClass UnderlyingProvider { get; }
        public string Name => UnderlyingProvider.Name;

        // Список дочерних свойств для отображения в дереве
        public ObservableCollection<PropertyUiWrapper> Properties { get; }

        public ProviderUiWrapper(SelectedDataClass provider)
        {
            UnderlyingProvider = provider;

            // Извлекаем свойства из C++ Dictionary и оборачиваем их
            var props = provider.AllProperties?
                .Select(kvp => new PropertyUiWrapper(kvp.Key, kvp.Value))
                .ToList() ?? new System.Collections.Generic.List<PropertyUiWrapper>();

            Properties = new ObservableCollection<PropertyUiWrapper>(props);
        }
    }
}
