using Autodesk.AutoCAD.Runtime;
using BaseFunction;
using System.Collections.Generic;

namespace CurveProperties.Ribbon
{
    public class Commands
    {
        public static List<CommandGroup> CommandClasses = new List<CommandGroup>
        {
            ////для сплит кнопки
            //new CommandGroup("названиеРазедлаЛенты", "названиеПанелиНаЛенте",
            //    new List<CommandItem>
            //    {
            //        new CommandItem("названиеКоманды1","Команда1","описаниеКоманды1",
            //            //запуск метода
            //            () => { System.Windows.MessageBox.Show("test1"); }),
            //        new CommandItem("названиеКоманды2","Команда2","описаниеКоманды2",
            //            //запуск метода
            //            () => { System.Windows.MessageBox.Show("test2"); }, CommandFlags.UsePickSet | CommandFlags.Redraw)
            //    }
            //),
            ////для обычной кнопки
            //new CommandGroup("названиеРазедлаЛенты", "названиеПанелиНаЛенте",
            //       new CommandItem("названиеКоманды3","Команда3","описаниеКоманды3",
            //            //запуск метода
            //            () => { System.Windows.MessageBox.Show("test3"); }, CommandFlags.UsePickSet | CommandFlags.Redraw)
            //)
        };
    }
}
