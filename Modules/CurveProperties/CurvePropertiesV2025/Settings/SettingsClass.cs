using BaseFunction;

namespace CurveProperties
{
    public class SettingsClass : BaseClass
    {
        public bool UseBaseCircle { get => _UseBaseCircle; set { SetData(ref _UseBaseCircle, value); } }
        private bool _UseBaseCircle = false;

    }
}