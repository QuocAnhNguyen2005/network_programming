- Set up environment in visual studio code
  - Install QT and set up environment variables
  - Install C++ extension in VS Code

* run client
  g++ client.cpp -o client.exe -lws2_32
  client.exe
* run server
  g++ server.cpp -o server.exe -lws2_32
  server.exe
* run UI app
  Ctrl+Shift+P -> Cmake: Select Kit -> Choose your compiler in QT
  Ctrl+Shift+P -> Cmake: Configure
  Ctrl+Shift+P -> Cmake: Build
  Ctrl+Shift+P -> Cmake: Run Without Debugging
