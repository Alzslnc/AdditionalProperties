using Autodesk.AutoCAD.DatabaseServices;
using Autodesk.AutoCAD.Geometry;
using ArxBridge;
using System;
using System.Collections.Generic;
using static BaseFunction.BaseGeometryClass;
using ADB = Autodesk.AutoCAD.DatabaseServices;
using SWS = System.Windows.Shapes;
using SWM = System.Windows.Media;

namespace CurveProperties
{
    public class CurvePropertiesDataClass : SelectedDataClass
    {
        public CurvePropertiesDataClass() 
        {
            Name = "Данные кривых";
            AllProperties = new Dictionary<string, bool> 
            {            
                { "Угол", true },
                { "Вертикальный угол", true },             
                { "Длина (сумма)", true },              
                { "Площадь (сумма)", true }, 
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
                if (e is Line l && (AllProperties["Угол"] || AllProperties["Вертикальный угол"]))
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
                    if (AllProperties["Угол"]) o.Properties.Add(new Property("Угол", angle.ToString(), false, this) { Group = "Параметры" });
                    if (AllProperties["Вертикальный угол"]) o.Properties.Add(new Property("Вертикальный угол", vAngle.ToString(), false, this) { Group = "Параметры" });
                }

             

                if (AllProperties["Длина (сумма)"])
                {
                    double length = curve.GetLength();    
                    if (AllProperties["Длина (сумма)"]) o.Properties.Add(new Property("Длина (сумма)", length.ToString(), true, this) { Group = "Геометрия", IsSum = true });
                }

                if (AllProperties["Площадь (сумма)"])
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
                    if (AllProperties["Площадь"]) o.Properties.Add(new Property("Площадь", area.ToString(), true, this) { Group = "Геометрия", Error = error });
                    if (AllProperties["Площадь (сумма)"]) o.Properties.Add(new Property("Площадь (сумма)", area.ToString(), true, this) { Group = "Геометрия", IsSum = true, Error = error });
                }       
            }         
        }
        public override void SetProperties(ADB.Entity e, List<Property> properties)
        {            
            foreach (Property property in properties)
            {
                if (!property.IsChanged) continue;

                switch (property.Name)
                {                  
                    case "Угол":
                        {
                            if (double.TryParse(property.NewValue.Replace(",","."), out double d) && e is Line l)
                            {
                                while (d > 360) d -= 360;
                                while (d < 0) d += 360;

                                d = d * Math.PI / 180;

                                double angle = 0;

                                Vector3d direction = l.EndPoint - l.StartPoint;
                                if (!direction.IsZeroLength())
                                {
                                    angle = Vector3d.XAxis.GetAngleTo(new Vector3d(direction.X, direction.Y, 0), Vector3d.ZAxis);
                                }

                                d -= angle;

                                l.TransformBy(Matrix3d.Rotation(d, Vector3d.ZAxis, l.StartPoint));
                            }
                            break;
                        }
                    case "Вертикальный угол":
                        {
                            if (double.TryParse(property.NewValue.Replace(",", "."), out double d) && e is Line l)
                            {
                                while (d > 360) d -= 360;
                                while (d < 0) d += 360;

                                d = d * Math.PI / 180;

                                double vAngle = 0;

                                Vector3d direction = l.EndPoint - l.StartPoint;
                                Vector3d proj;
                                Vector3d rotDir = -Vector3d.YAxis;
                                if (!direction.IsZeroLength())
                                {
                                    proj = new Vector3d(direction.X, direction.Y, 0);
                                    if (proj.IsZeroLength()) proj = Vector3d.XAxis;
                                    rotDir = proj.TransformBy(Matrix3d.Rotation(-Math.PI / 2, Vector3d.ZAxis, Point3d.Origin));
                                    vAngle = proj.GetAngleTo(direction, rotDir);                                   
                                }

                                d -= vAngle;

                                l.TransformBy(Matrix3d.Rotation(d, rotDir, l.StartPoint));
                            }
                            break;
                        }              

                }
            }            
        }  
    }
}
