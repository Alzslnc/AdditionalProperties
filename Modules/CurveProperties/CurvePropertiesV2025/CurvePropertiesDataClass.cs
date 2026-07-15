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
                { "Угол", new PropertyMeta("Угол", "Угол наклона кривой в плоскости X-Y", PropType.IsDouble) },
                { "Вертикальный угол", new PropertyMeta("Вертикальный угол", "Угол наклона кривой относительно плоскости X-Y", PropType.IsDouble) },
                { "Длина (сумма)", new PropertyMeta("Длина (сумма)", "Суммарная длина всех выделенных кривых", PropType.IsDouble) },
                { "Площадь (сумма)", new PropertyMeta("Площадь (сумма)", "Суммарная площадь замкнутых контуров", PropType.IsDouble) },
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
                if (e is Line l && (AllProperties["Угол"].IsEnabled || AllProperties["Вертикальный угол"].IsEnabled))
                {
                    double angle = 0;
                    double vAngle = 0;
                    Vector3d direction = (l.EndPoint - l.StartPoint).GetNormal();
                    Vector3d proj = new Vector3d(direction.X, direction.Y, 0);

                    if (proj.IsZeroLength()) proj = Vector3d.XAxis;

                    if (!direction.IsZeroLength())
                    {
                        angle = Vector3d.XAxis.GetAngleTo(proj, Vector3d.ZAxis) * 180 / Math.PI;
                        if (direction.Z != 0)
                        {
                            Vector3d rotDir = proj.TransformBy(Matrix3d.Rotation(-Math.PI / 2, Vector3d.ZAxis, Point3d.Origin));
                            vAngle = proj.GetAngleTo(direction, rotDir) * 180 / Math.PI;
                        }
                    }

                    if (AllProperties["Угол"].IsEnabled) o.Properties.Add(new Property("Угол", angle.ToString(CultureInfo.InvariantCulture), false, this) { Group = "Параметры" });
                    if (AllProperties["Вертикальный угол"].IsEnabled) o.Properties.Add(new Property("Вертикальный угол", vAngle.ToString(CultureInfo.InvariantCulture), false, this) { Group = "Параметры" });
                }

                // Расчет длины (Убрали дублирующийся if)
                if (AllProperties["Длина (сумма)"].IsEnabled)
                {
                    double length = curve.GetLength();
                    o.Properties.Add(new Property("Длина (сумма)", length.ToString(CultureInfo.InvariantCulture), true, this) { Group = "Геометрия", IsSum = true });
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
                                      
                    if (AllProperties["Площадь (сумма)"].IsEnabled) o.Properties.Add(new Property("Площадь (сумма)", areaStr, true, this) { Group = "Геометрия", IsSum = true, Error = error });
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
                    case "Вертикальный угол":
                        {
                            if (double.TryParse(valStr, NumberStyles.Any, CultureInfo.InvariantCulture, out double d) && e is Line l)
                            {
                                // ИСПРАВЛЕНО: Безопасная нормализация
                                d %= 360;
                                if (d < 0) d += 360;

                                d = d * Math.PI / 180;
                                double vAngle = 0;

                                Vector3d direction = l.EndPoint - l.StartPoint;
                                Vector3d rotDir = -Vector3d.YAxis;
                                if (!direction.IsZeroLength())
                                {
                                    Vector3d proj = new Vector3d(direction.X, direction.Y, 0);
                                    if (proj.IsZeroLength()) proj = Vector3d.XAxis;
                                    rotDir = proj.TransformBy(Matrix3d.Rotation(-Math.PI / 2, Vector3d.ZAxis, Point3d.Origin));
                                    vAngle = proj.GetAngleTo(direction, rotDir);
                                }

                                l.TransformBy(Matrix3d.Rotation(d - vAngle, rotDir, l.StartPoint));
                            }
                            break;
                        }
                }
            }
        }
    }
}
