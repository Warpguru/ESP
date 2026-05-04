# Iteration 5: Network connectivity

## Goal

Goal is that after flashing the ESP spawns an Wifi Access Point that allows the user to input network details.
Upon a successful connection the data is stored in the ESP's persistent storage so the next time it is powered on that connection info can be used to directly connect to the Wifi network.

## 1. Implementation details

### A. Initial Access Point

After flashing the ESP it spawns an Wifi Access Point `SerialController` that serves at the context root `/` a web page that allows the user to input (or better select from a list of the available networks) the network's SSID, the userid and the password.
Upon a successful connection to a Wifi network with these credentials, these credentials will be persisted in the ESP's persistent storage (a storage area on the ESP that survives permanent power outages).

### B. Reuse

When booting the applications tries to read the credentials form the ESP's persistent storage and automatically tries to connect to the network.
Only if:
  - no credentials are in ESP's persistent storage
  - the credentials are no longer valid to connect to a network
the application will again continue with `A. Initial Access Point`.

### C. Configuration update

And API `/reset` must be implemented that deletes the credentials from ESP's persistent storage.
The next reboot the application will again start with `A. Initial Access Point`.

## Next Step
Continue to **Iteration 6.md** to configure web interface with real-time updates.
