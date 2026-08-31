#pragma once

class OpmMessageHook {
public:
    // Флаг модификации объекта (взводится в CacheInvalidationSubscription)
    static bool IsChanged;

    // Инициализация хука (теперь без параметров, так как коллбэк встроен внутри)
    static void Start();

    // Принудительное снятие хука при выгрузке плагина (.arx) из AutoCAD
    static void Stop();
};
