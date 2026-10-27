using BaseFunction;
using System.Collections.Generic;

namespace ArxBridgeUI.Ribbon
{
    public class Commands
    {
        public static List<CommandGroup> CommandClasses = new List<CommandGroup>
        {
            //для обычной кнопки
            new CommandGroup("nCommand", "Панель свойств",
                   new CommandItem("ArxBridgeSettings","Настройки","Параметры вывода дополнительных свойств в панель свойств автокада",
                        //запуск метода
                        () => { ArxBridgeUI.UI.SettingsViewModel.Launch(); })
            )
        };
    }
}
