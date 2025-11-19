# Pause My Aircraft
### Automatically pause your simulator based on Zulu Time, Waypoint Distance, or Top of Descent

**Version:** 1.0.0  
**Author:** Joaquim Manjama  
**Platform:** Windows  
**License:** MIT  

Pause My Aircraft is a utility plugin for **X-Plane** that pauses the simulator when certain user-defined conditions are met.  
This is especially useful for long-haul flights, unattended cruise segments, or flights where the pilot wants to ensure they don’t miss important phases like TOD or waypoint transitions.

---

## ✈️ Features

### **1. Zulu Time Mode**
Set a target UTC time (HH:MM).  
When the simulator’s Zulu time reaches the selected time, the sim pauses automatically.

---

### **2. Waypoint Mode**
Specify:
- A waypoint name  
- A distance in nautical miles  

The plugin pauses the sim when the aircraft is within the target distance of the selected waypoint.

---

### **3. Top of Descent (TOD) Mode**
Enter:
- Cruising flight level (FL)
- Destination airport ICAO code

The plugin computes an estimated TOD point using a **3-degree descent path** adjusted with a safety margin, and pauses the sim when reached.

---

## 📦 Installation

1. Download the latest release from GitHub.
2. Extract the folder.
3. Move it to: X-Plane\Resources\plugins
