/*
 * src/asc.c
 *
 * AppServer Library
 *
 * Copyright (C) 2014  Anass Bouassaba
 *
 */

#include <Asl/Connector.h>
#include <sstream>
#include <unistd.h>

#define OS_X 1
#define EXPORT __attribute__((visibility("default")))

int errno;

typedef struct Asp_Event Asp_Event;

using namespace asl;

// ------------------------------------ Event

Event::Event()
    : _windowId(AspUndefinedWindowId), _eventType(kEventTypeUndefined)
{
}

Event::Event(TWindowId id, EventType type)
    : _windowId(id), _eventType(type)
{
}

void Event::setWindowId(TWindowId id)
{
    _windowId = id;
}

TWindowId Event::getWindowId() const
{
    return _windowId;
}

void Event::setEventType(EventType type)
{
    _eventType = type;
}

EventType Event::getEventType() const
{
    return _eventType;
}

Event::~Event()
{
}

// ------------------------------------ InputEvent

InputEvent::InputEvent()
    : Event(), _inputEventType(kInputEventTypeUndefined)
{
}

InputEvent::InputEvent(TWindowId id, InputEventType type)
    : Event(id, kEventTypeInput), _inputEventType(type)
{
}

void InputEvent::setInputEventType(InputEventType type)
{
    _inputEventType = type;
}

InputEventType InputEvent::getInputEventType() const
{
    return _inputEventType;
}

InputEvent::~InputEvent()
{
}

// ------------------------------------ WindowLocationChangedEvent

WindowLocationChangedEvent::WindowLocationChangedEvent(const Asp_Event& aspEvent)
: Event(aspEvent.winId, kEventTypeWindowLocationChanged), _x(aspEvent.field0), _y(aspEvent.field1)
{
}

WindowLocationChangedEvent::WindowLocationChangedEvent()
: Event(), _x(0.0), _y(0.0)
{
}

WindowLocationChangedEvent::WindowLocationChangedEvent(TWindowId id, double x, double y)
: Event(id, kEventTypeWindowLocationChanged), _x(x), _y(y)
{
}

void WindowLocationChangedEvent::setNewWindowX(double x)
{
    _x = x;
}

void WindowLocationChangedEvent::setNewWindowY(double y)
{
    _y = y;
}

double WindowLocationChangedEvent::getNewWindowX() const
{
    return _x;
}

double WindowLocationChangedEvent::getNewWindowY() const
{
    return _y;
}

WindowLocationChangedEvent::~WindowLocationChangedEvent()
{
}

// ------------------------------------ MouseEvent

MouseEvent::MouseEvent()
    : InputEvent(AspUndefinedWindowId, kInputEventTypeMouse),
    _mouseEventType(kMouseEventTypeUndefined), _button(kMouseButtonUndefined), _x(0.0f), _y(0.0f), _absX(0.0f), _absY(0.0f)
{
}

MouseEvent::MouseEvent(TWindowId id, MouseEventType type, MouseButton button, double x, double y, double absX, double absY)
    : InputEvent(id, kInputEventTypeMouse), _mouseEventType(type), _button(button), _x(x), _y(y), _absX(absX), _absY(absY)
{
}

MouseEvent::MouseEvent(const Asp_Event& aspEvent)
    : InputEvent(AspUndefinedWindowId, kInputEventTypeMouse)
{
    setWindowId(aspEvent.winId);
    
    if (aspEvent.field4 == AspMouseEventMove) {
        setMouseEventType(kMouseEventTypeMove);
        setMouseButton(kMouseButtonUndefined);
    }
    else if (aspEvent.field4 == AspMouseEventPress) {
        setMouseEventType(kMouseEventTypePress);
        setMouseButton(parseMouseButton(aspEvent.field5));
    }
    else if (aspEvent.field4 == AspMouseEventRelease) {
        setMouseEventType(kMouseEventTypeRelease);
        setMouseButton(parseMouseButton(aspEvent.field5));
    }
    else if (aspEvent.field4 == AspMouseEventDrag) {
        setMouseEventType(kMouseEventTypeDrag);
        setMouseButton(parseMouseButton(aspEvent.field5));
    }
    
    setX(aspEvent.field0);
    setY(aspEvent.field1);
    setAbsX(aspEvent.field2);
    setAbsY(aspEvent.field3);
}

MouseButton MouseEvent::parseMouseButton(unsigned int aspMouseButton)
{
    if (aspMouseButton == AspMouseButtonRight) {
        return kMouseButtonRight;
    }
    else if (aspMouseButton == AspMouseButtonMiddle) {
        return kMouseButtonMiddle;
    }
    else if (aspMouseButton == AspMouseButtonLeft) {
        return kMouseButtonLeft;
    }
    
    return kMouseButtonUndefined;
}

void MouseEvent::setMouseEventType(MouseEventType type)
{
    _mouseEventType = type;
}

MouseEventType MouseEvent::getMouseEventType() const
{
    return _mouseEventType;
}

void MouseEvent::setMouseButton(MouseButton button)
{
    _button = button;
}

MouseButton MouseEvent::getMouseButton() const
{
    return _button;
}

void MouseEvent::setX(double x)
{
    _x = x;
}

double MouseEvent::getX() const
{
    return _x;
}

void MouseEvent::setY(double y)
{
    _y = y;
}

double MouseEvent::getY() const
{
    return _y;
}

void MouseEvent::setAbsX(double absX)
{
    _absX = absX;
}

double MouseEvent::getAbsX() const
{
    return _absX;
}

void MouseEvent::setAbsY(double absY)
{
    _absY = absY;
}

double MouseEvent::getAbsY() const
{
    return _absY;
}

MouseEvent::~MouseEvent()
{
}

// ------------------------------------ KeyEvent

KeyEvent::KeyEvent()
{
}

KeyEvent::KeyEvent(const Asp_Event& aspEvent, std::string text)
    : InputEvent(AspUndefinedWindowId, kInputEventTypeKey)
{
    setWindowId(aspEvent.winId);
    _text = text;
}

void KeyEvent::setKeyEventType(KeyEventType type)
{
    _keyEventType = type;
}

KeyEventType KeyEvent::getKeyEventType() const
{
    return _keyEventType;
}

void KeyEvent::setText(std::string text)
{
    _text = text;
}

std::string KeyEvent::getText() const
{
    return _text;
}

KeyEvent::~KeyEvent()
{    
}

// ------------------------------------ Connector

/*
 * Initiates a socket connection with the server.
 *
 * cl: client
 * return: Negative number if it fails
 */
Connector::Connector()
{  
    _context = std::make_shared<zmq::context_t>(1);
    
    // Registration socket
    _regSocket = std::make_shared<zmq::socket_t>(*_context.get(), ZMQ_REQ);
    _regSocket->connect("tcp://localhost:9000");

    // Process Monitor socket
    _processMonitorSocket = std::make_shared<zmq::socket_t>(*_context.get(), ZMQ_REQ);
    _processMonitorSocket->connect("tcp://localhost:9001");
}

/*
 * Register the client with the server.
 * Before calling this function, the client won't be able to do any operation on the server.
 */
void Connector::subscribe()
{
    TProcId pid = getpid();

    // Subscribe to appserver
    Asp_Request subscribeRequest;
    subscribeRequest.type = AspRequestRegister;
    subscribeRequest.field0 = pid;
    zmq::message_t request(&subscribeRequest, sizeof(subscribeRequest));
    _regSocket->send(request);

    Asp_Event evt;
    size_t receivedSize = _regSocket->recv(&evt, sizeof(evt));
    if (receivedSize > 0) {
        _clientId = evt.field0;
        std::cout << "Received client ID: " << _clientId << std::endl;

        // Notify the process monitor
        Asp_SubscribeRequest req;
        req.pid = pid;
        req.clientId = _clientId;
        zmq::message_t processMonitorRequest(&req, sizeof(req));
        _processMonitorSocket->send(processMonitorRequest);

        // ACK
        int ack = 0;
        receivedSize = _processMonitorSocket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            exit(1);
        }

        std::cout << "Notified process manager" << std::endl;

        // Request socket
        _socket = std::make_shared<zmq::socket_t>(*_context.get(), ZMQ_REQ);
        std::stringstream reqSocketAddress;
        reqSocketAddress << "tcp://localhost:";
        int reqSocketPort = AspReqListenerThreadPortValue + _clientId;
        reqSocketAddress << reqSocketPort;
        _socket->connect(reqSocketAddress.str());
        std::cout << "Connected to request socket in port " << reqSocketPort << std::endl;

        // Events socket
        int port = 10000 + _clientId;
        _eventsSocket = std::make_shared<zmq::socket_t>(*_context.get(), ZMQ_REP);
        std::stringstream address;
        address << "tcp://*:";
        address << port;
        _eventsSocket->bind(address.str());
        std::cout << "Events socket server started in port " << port << std::endl;
    }
    else {
        exit(1);
    }
}

/*
 * Create a window on the server with a initial raster data
 *
 * field0   : X
 * field1   : Y
 * field2   : Width
 * field3   : Height
 * field4   : Visibility
 *
 * return: ID of the created window
 */

TWindowId Connector::newWindow(unsigned char *data, unsigned long dataSize, double x, double y, double width, double height, int rasterType, bool visible)
{
    try {
        Asp_Request createWindowReq;
        createWindowReq.type = AspRequestCreateWindow;
        createWindowReq.clientId = _clientId;
        createWindowReq.field0 = x;
        createWindowReq.field1 = y;
        createWindowReq.field2 = width;
        createWindowReq.field3 = height;
        createWindowReq.field4 = (bool)visible;
        createWindowReq.field5 = (int)rasterType;
        createWindowReq.dataSize = dataSize;
        
        // Send request
        zmq::message_t request(&createWindowReq, sizeof(Asp_Request));
        _socket->send(request);
        
        // ACK
        int ack = 0;
        size_t receivedSize = _socket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            return -1;
        }
        std::cout << "newWindow: got ACK for newWindow" << std::endl;
        
        // Send raster data
        const void* castedData = (const void*)data;
        zmq::message_t dataRequest(castedData, (size_t)dataSize);
        _socket->send(dataRequest);
        
        std::cout << "newWindow: data sent" << std::endl;
        
        Asp_Event evt;
        evt.winId = 0;
        unsigned int priority;
        bool foundExistingId = false;
        
        do  {
            size_t receivedSize = _socket->recv(&evt, sizeof(evt));
            if (receivedSize <= 0) return -1;
            
            foundExistingId = false;
            for (int i = 0; i < _windowIds.size(); i++) {
                TWindowId id = _windowIds.at(i);
                if (evt.winId == id) {
                    foundExistingId = true;
                    break;
                }
            }
            
            if (foundExistingId) {
                std::cout << __func__ << ": " << "Got the same window_id like previous time. Iterating again..." << std::endl;
            }
            else if (evt.winId == 0) {
                std::cout << __func__ << ": " << "Window_id equals 0. Iterating again..." << std::endl;
            }
        } while (foundExistingId || evt.winId == 0);
        
        std::cout << "got window ID: " << evt.winId << std::endl;
        _windowIds.push_back(evt.winId);
        
        return evt.winId;
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
        return -1;
    }
    return -1;
}

/*
 * Update window's raster in a specific region.
 * The width and height region should match the arguments width and height given to the function
 *
 * field1   : X
 * field2   : Y
 * field3   : Width
 * field4   : Height
 */
void Connector::updateWindowSurface(TWindowId id, unsigned char *data, unsigned long dataSize, double x, double y, double width, double height)
{
    try {
        Asp_Request req;
        req.type = AspRequestUpdateWindowSurface;
        req.clientId = _clientId;
        req.winId = id;
        req.field1 = x;
        req.field2 = y;
        req.field3 = width;
        req.field4 = height;
        req.dataSize = dataSize;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
        
        // ACK
        int ack = 0;
        size_t receivedSize = _socket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            return;
        }
        
        // Send raster data
        const void* castedData = (const void*)data;
        zmq::message_t dataRequest(castedData, (size_t)dataSize);
        _socket->send(dataRequest);
        
        // ACK
        ack = 0;
        receivedSize = _socket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            return;
        }
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * Resize window
 *
 * field1   : Width
 * field2   : Height
 */
void Connector::resizeWindow(TWindowId id, unsigned char *data, unsigned long dataSize, double width, double height)
{
    try {
        Asp_Request req;
        req.type = AspRequestResizeWindow;
        req.clientId = _clientId;
        req.winId = id;
        req.field1 = width;
        req.field2 = height;
        req.dataSize = dataSize;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
        
        // ACK
        int ack = 0;
        size_t receivedSize = _socket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            return;
        }
        
        // Send raster data
        const void* castedData = (const void*)data;
        zmq::message_t dataRequest(castedData, (size_t)dataSize);
        _socket->send(dataRequest);
        
        // ACK
        ack = 0;
        receivedSize = _socket->recv(&ack, sizeof(int));
        if (receivedSize <= 0 || ack != 1) {
            return;
        }
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * Show or hide a window
 *
 * field1 : Visiblility
 */
void Connector::changeWindowVisiblity(TWindowId id, bool visible)
{
    try {
        Asp_Request req;
        req.type = AspRequestSetWindowVisibility;
        req.clientId = _clientId;
        req.winId = id;
        req.field1 = visible;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * Make a window top level
 */
void Connector::bringWindowToFront(TWindowId id)
{
    try {
        Asp_Request req;
        req.type = AspRequestBringWindowToFront;
        req.clientId = _clientId;
        req.winId = id;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * Change window's location
 *
 * field1 : X
 * field2 : Y
 */
void Connector::moveWindow(TWindowId id, double x, double y)
{
    try {
        Asp_Request req;
        req.type = AspRequestMoveWindow;
        req.clientId = _clientId;
        req.winId = id;
        req.field1 = x;
        req.field2 = y;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * Destroy window
 */
void Connector::destroyWindow(TWindowId id)
{
    try {
        Asp_Request req;
        req.type = AspRequestDestroyWindow;
        req.clientId = _clientId;
        req.winId = id;
        
        // Send request
        zmq::message_t request(&req, sizeof(Asp_Request));
        _socket->send(request);
    }
    catch (std::exception e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
}

/*
 * A blocker function that returns an input event from the server when available
 *
 * return: An event from the server
 */
std::shared_ptr<Event> Connector::waitEvent()
{
    try {
        Asp_Event req;
        size_t receivedSize = _eventsSocket->recv(&req, sizeof(Asp_Event));
        if (receivedSize == 0) {
            exit(1);
        }
        
        int ack = 1;
        zmq::message_t ackResponse(&ack, sizeof(int));
        _eventsSocket->send(ackResponse);
        
        if (req.type == AspEventMouseInput) {
            return std::make_shared<MouseEvent>(req);
        }
        else if (req.type == AspEventTextInput) {
            char *text = (char*)malloc(req.field5);
            receivedSize = _eventsSocket->recv(text, req.field5);
            if (receivedSize == 0) {
                exit(1);
            }
            
            ack = 1;
            zmq::message_t ackResponse2(&ack, sizeof(int));
            _eventsSocket->send(ackResponse2);
            
            return std::make_shared<KeyEvent>(req, text);
        }
        else if (req.type == AspEventWindowLocationChanged) {
            return std::make_shared<WindowLocationChangedEvent>(req);
        }
    }
    catch (zmq::error_t e) {
        std::cout << __func__ << ": " << e.what() << std::endl;
    }
    
    return nullptr;
}

void Connector::unsubscribe()
{    
    _socket->close();
}

Connector::~Connector()
{
}

