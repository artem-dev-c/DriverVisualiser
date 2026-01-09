\# Driver Visualiser



\*\*Driver Visualiser\*\* is a Windows desktop application for inspecting, analyzing, and visualizing installed device drivers and their relationship to hardware devices.



The project focuses on \*\*correct Windows Plug and Play (PnP) modeling\*\*, \*\*driver–device binding\*\*, and \*\*human-readable diagnostics\*\*, rather than simple driver listing.



> Status: \*\*Work in progress (~10%)\*\*  

> Core architecture and driver scanning are implemented. Device modeling and advanced grouping are under active redesign.



---



\## Goals



\- Provide a clear, structured view of Windows devices and their drivers  

\- Expose driver health, importance, and relevance  

\- Avoid heuristic or name-based grouping  

\- Build a solid foundation for future diagnostics and visualization  



---



\## Current Features



\### Driver Scanning

\- Enumerates installed drivers using Windows SetupAPI

\- Collects:

&nbsp; - Driver name

&nbsp; - Version

&nbsp; - Install date

&nbsp; - Manufacturer

&nbsp; - Status

&nbsp; - Device Instance ID (PnP binding)



\### Device–Driver Grouping (v1)

\- Groups drivers by \*\*Device Instance ID\*\*

\- Each device is represented as a Plug and Play node

\- No string-based or vendor-based guessing



\### Driver Importance Evaluation

Each driver is assigned an importance level:



\- \*\*Critical\*\* – required for core system or hardware operation  

\- \*\*Recommended\*\* – important but non-blocking  

\- \*\*Optional\*\* – auxiliary or vendor-specific  



\### Driver Fault / Risk Rating

Drivers are evaluated using a fault likelihood score:



\- Range: \*\*0–100\*\* (step 10)

\- Based on:

&nbsp; - Driver status

&nbsp; - Install age

&nbsp; - Early vendor heuristics



> This score is an analytical indicator, not a guarantee of failure.



\### User Interface

\- Tree-based view: \*\*Device → Drivers\*\*

\- Human-readable formatting for:

&nbsp; - Status

&nbsp; - Versions

&nbsp; - Dates

&nbsp; - Importance



---



\## Architecture Overview



The application follows a layered design:



Windows APIs (SetupAPI / ConfigMgr)

↓

DriverScanner

↓

Device / Driver Model

↓

Evaluators \& Formatters

↓

Qt UI



\### Design Principles



\- Devices are primary entities, drivers are secondary

\- Relationships are derived from PnP data

\- UI consumes data but does not make classification decisions

\- Avoid premature abstraction or over-engineering



---



\## Technologies Used



\- C++20

\- Qt (Widgets)

\- Windows SetupAPI / ConfigMgr

\- STL



---



\## Planned Work



\- \[ ] Device node abstraction based on DEVINST

\- \[ ] Composite device modeling (parent / sibling relations)

\- \[ ] Driver role classification (function / filter / software)

\- \[ ] Improved fault scoring model

\- \[ ] Export to JSON or report

\- \[ ] UI filtering and sorting

\- \[ ] Device health overview



---



\## Project Status



The current codebase represents a \*\*foundational architecture\*\*, not a finished product.





