using ArxBridge;
using Autodesk.AutoCAD.DatabaseServices;
using Autodesk.AutoCAD.Geometry;
using System;
using System.Collections.Generic;
using System.Globalization;
using ADB = Autodesk.AutoCAD.DatabaseServices;

namespace CurveProperties
{
    public class CurvePropertiesDataClass : SelectedDataClass
    {    
        //в конструкторе создаем метаданные для регистрации свойств и записываем их в соответствующий список
        public CurvePropertiesDataClass() 
        {
            Name = "Данные кривых";          
            AllProperties = new Dictionary<string, PropertyMeta>
            {
                { 
                    "Угол", 
                    new PropertyMeta("Угол", "Угол наклона кривой в плоскости X-Y", PropType.IsDouble) 
                    { 
                        //типы у которых будет отображаться данное свойство
                        Types = new List<Type>{ typeof(Line) }, 
                        //группа, для стандартных можно выбрать не Custom, для своей выбрать Custom и название
                        GroupType = PropertyGroup.Custom, 
                        CustomGroupName = "Геометрия" 
                    } },
                { 
                    "Длина (сумма)", 
                    new PropertyMeta("Длина (сумма)", "Суммарная длина всех выделенных кривых", PropType.IsDouble) 
                    { 
                        Types = new List<Type>{ typeof(Curve) }, 
                        GroupType = PropertyGroup.Custom, 
                        CustomGroupName = "Группировка"  
                    } },
                { 
                    "Площадь (сумма)", 
                    new PropertyMeta("Площадь (сумма)", "Суммарная площадь замкнутых контуров", PropType.IsDouble) 
                    { 
                        Types = new List<Type>{ typeof(Curve) }, 
                        GroupType = PropertyGroup.Custom, 
                        CustomGroupName = "Группировка"   
                    }  },
            };            
        }
        //метод срабатывающий один раз перед получением параметров объекта, в нем можно например задать точность округления значений в панели
        public override void PreAcadUpdateAction()
        {
            
            object luprec = Autodesk.AutoCAD.ApplicationServices.Application.GetSystemVariable("LUPREC");

            if (luprec != null && int.TryParse(luprec.ToString(), out int result))
            {
                Precision = result;
            }           
           
        }
        /// <summary>
        /// метод вызываемый при выборе объекта, в нем мы реализуем методику получения данных с выбранных объектов
        /// в SelectedObjectData надо добавить объкты Property с считанными данными, 
        /// нужно контролировать какой объект прилетает от моста, так как он она посылает все, которые выбраны,
        /// объект прилетающий от моста в этом методе открыт на чтение 
        /// </summary>
        /// <param name="e"></param>
        /// <param name="o"></param>
        public override void GetProperties(ADB.Entity e, ArxBridge.SelectedObjectData o, Transaction tr)
        {
            if (e is Curve curve && IsAcadCurve(curve))
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

                    if (AllProperties["Угол"].IsEnabled) o.Properties.Add(new Property(AllProperties["Угол"], angle.ToString(CultureInfo.InvariantCulture), false, this));
                }

                // Расчет длины
                if (AllProperties["Длина (сумма)"].IsEnabled)
                {
                    double length = GetLength(curve);
                    o.Properties.Add(
                        new Property(AllProperties["Длина (сумма)"], length.ToString(CultureInfo.InvariantCulture), true, this) 
                        { 
                            //так как параметрт должен просуммироваться в панели ставим этот флаг
                            IsSum = true 
                        });
                }

                // Расчет площади
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
        /// <summary>
        /// метод изменения данных объекта при изменении их в панели
        /// мост присылает объект, открытый на запись и список свойств объекта
        /// из всех свойств выбираете нужное, получаете его новое значение от панели и меняете
        /// для возможности не заморачиваться с точками и запятыми от панели приходит текст, который надо парсить
        /// </summary>
        /// <param name="e"></param>
        /// <param name="properties"></param>
        public override void SetProperties(ADB.Entity e, List<Property> properties, Transaction tr)
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
        private static bool IsAcadCurve(Entity ent, bool includeLeader = false)
        {
            if (ent is Arc || ent is Circle || ent is Ellipse || ent is Line ||
                ent is Polyline || ent is Polyline2d || ent is Polyline3d ||
                ent is Ray || ent is Spline || ent is Xline || (includeLeader && ent is Leader)) return true;
            else return false;
        }
        private static double GetLength(Curve curve)
        {
            if (curve == null || curve.IsDisposed || curve.IsErased) return 0;
            try
            {
                return curve.GetDistanceAtParameter(curve.EndParam) - curve.GetDistanceAtParameter(curve.StartParam);
            }
            catch { return 0; }
        }
    }
}
