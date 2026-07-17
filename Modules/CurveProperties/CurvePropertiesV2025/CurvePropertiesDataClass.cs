using ArxBridge;
using Autodesk.AutoCAD.DatabaseServices;
using Autodesk.AutoCAD.Geometry;
using BaseFunction;
using System;
using System.Collections.Generic;
using System.Globalization; // Добавили для безопасного парсинга чисел
using ADB = Autodesk.AutoCAD.DatabaseServices;

namespace CurveProperties
{
    public class CurvePropertiesDataClass : SelectedDataClass
    {
        public CurvePropertiesDataClass()
        {
            Name = "Данные кривых";

            // Переводим AllProperties на новый класс метаданных!
            AllProperties = new Dictionary<string, PropertyMeta>
            {
                { "Угол", new PropertyMeta("Угол", "Угол наклона кривой в плоскости X-Y", PropType.IsDouble) { Types = new List<Type>{ typeof(Line) }, GroupType = PropertyGroup.Custom, CustomGroupName = "Странное" } },
                { "Длина (сумма)", new PropertyMeta("Длина (сумма)", "Суммарная длина всех выделенных кривых", PropType.IsDouble) { Types = new List<Type>{ typeof(Curve) }, GroupType = PropertyGroup.Custom, CustomGroupName = "Группировка"  } },
                { "Площадь (сумма)", new PropertyMeta("Площадь (сумма)", "Суммарная площадь замкнутых контуров", PropType.IsDouble) { Types = new List<Type>{ typeof(Curve) }, GroupType = PropertyGroup.Custom, CustomGroupName = "Группировка"   }  },
            };
        }

        public override void PreAcadUpdateAction()
        {
            object luprec = Autodesk.AutoCAD.ApplicationServices.Application.GetSystemVariable("LUPREC");
            if (luprec != null && int.TryParse(luprec.ToString(), out int result))
            {
                Precision = result;
            }
        }

        public override void GetProperties(ADB.Entity e, ArxBridge.SelectedObjectData o)
        {
            if (e is Curve curve && curve.IsAcadCurve())
            {
                // Расчет углов для Line
                if (e is Line l && (AllProperties["Угол"].IsEnabled))
                {
                    double angle = 0;                 
                    Vector3d direction = (l.EndPoint - l.StartPoint).GetNormal();
                    Vector3d proj = new Vector3d(direction.X, direction.Y, 0);

                    if (proj.IsZeroLength()) proj = Vector3d.XAxis;

                    if (!direction.IsZeroLength())
                    {
                        angle = Vector3d.XAxis.GetAngleTo(proj, Vector3d.ZAxis) * 180 / Math.PI;                       
                    }

                    if (AllProperties["Угол"].IsEnabled) o.Properties.Add(new Property(AllProperties["Угол"], angle.ToString(CultureInfo.InvariantCulture), false, this) );
                }

                // Расчет длины (Убрали дублирующийся if)
                if (AllProperties["Длина (сумма)"].IsEnabled)
                {
                    double length = curve.GetLength();
                    o.Properties.Add(new Property(AllProperties["Длина (сумма)"], length.ToString(CultureInfo.InvariantCulture), true, this) { IsSum = true });
                }

                // Расчет площади (Защита от разомкнутых кривых, не имеющих площади)
                if (AllProperties["Площадь (сумма)"].IsEnabled)
                {
                    double area = 0;
                    bool error = false;
                    try
                    {
                        area = curve.Area;
                    }
                    catch
                    {
                        error = true;
                    }

                    string areaStr = area.ToString(CultureInfo.InvariantCulture);
                                      
                    if (AllProperties["Площадь (сумма)"].IsEnabled) o.Properties.Add(new Property(AllProperties["Площадь (сумма)"], areaStr, true, this) { IsSum = true, Error = error });
                }
            }
        }

        public override void SetProperties(ADB.Entity e, List<Property> properties)
        {
            foreach (Property property in properties)
            {
                if (!property.IsChanged) continue;

                // Унифицируем строку для парсинга независимо от локали ПК пользователя
                string valStr = property.NewValue.Replace(",", ".");

                switch (property.Name)
                {
                    case "Угол":
                        {
                            if (double.TryParse(valStr, NumberStyles.Any, CultureInfo.InvariantCulture, out double d) && e is Line l)
                            {
                                // ИСПРАВЛЕНО: Безопасная нормализация угла без циклов while
                                d %= 360;
                                if (d < 0) d += 360;

                                d = d * Math.PI / 180;
                                double angle = 0;

                                Vector3d direction = l.EndPoint - l.StartPoint;
                                if (!direction.IsZeroLength())
                                {
                                    angle = Vector3d.XAxis.GetAngleTo(new Vector3d(direction.X, direction.Y, 0), Vector3d.ZAxis);
                                }

                                l.TransformBy(Matrix3d.Rotation(d - angle, Vector3d.ZAxis, l.StartPoint));
                            }
                            break;
                        }           
                }
            }
        }
    }
}
