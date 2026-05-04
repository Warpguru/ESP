# Threads

## Thread safety

Your Java and C++ instincts are 100% correct. Thread safety is arguably more critical on the ESP32 because you are dealing with two physical cores (Symmetric Multiprocessing - SMP) and direct hardware access.

Here is the breakdown of how to handle library thread safety on the ESP32:

1. Most Arduino Libraries are NOT Thread-Safe

Most libraries in the Arduino ecosystem were originally written for single-core AVR (Uno/Mega) chips. They often use global variables or static buffers. If you call modbus.read() from Core 0 and modbus.write() from Core 1 at the same time on the same instance, the internal state will likely corrupt, causing a crash or a "Guru Meditation Error."

2. The "Isolation Pattern" (The Pro Way)

Instead of trying to make a library thread-safe, the standard "Pro" architecture is to isolate the library to a single thread.

    * Task A (Core 0): "Owns" the WebServer object. No other thread ever touches it.
    * Task B (Core 1): "Owns" the ModBus object. No other thread ever touches it.
    * Communication: They talk to each other only through a Mutex-protected struct or a FreeRTOS Queue.

This is exactly what we did with your globalStats. The WebServer would simply read the globalStats (while holding the Mutex) and display them on a page, while the Modbus thread updates them.

3. The "Dangerous" Hardware Resources

On an ESP32, you must be particularly careful with shared hardware buses:
    * I2C / SPI: If you have an OLED on I2C and a Sensor on I2C, and you try to talk to them from different threads, the bus will lock up. You must use a Mutex to wrap the
     transaction.
    * Serial: As you saw, Serial.print from two cores simultaneously results in garbled text. While it won't crash the chip, it makes the output useless.
    * WiFi Stack: The underlying ESP-IDF WiFi stack is thread-safe (it runs its own internal task on Core 0), but the Arduino WiFiClient wrapper is generally not.

4. FreeRTOS Primitives are your Friends

Since the ESP32 runs FreeRTOS, you have powerful tools that Java developers would recognize:
    * Semaphores/Mutexes: What we used for the stats.
    * Queues: Excellent for passing messages (e.g., the WebServer puts a "Turn on Relay" command into a queue, and the Modbus task picks it up when it's ready).
    * Task Notifications: Lightweight signaling between threads.

## Summary for your Project:

Keep the WebServer on Core 0 (this is where the WiFi radio interrupts are handled by the system) and your Modbus on Core 1. Use a Mutex-protected struct to pass data between them. This keeps the code simple, prevents library corruption, and ensures that the slow Modbus timing doesn't make your website feel "laggy."
