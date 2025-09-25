# EventTemplates
You may find the details of the description of these 4 classes in https://a5w1h.blogspot.com/2025/06/event-handlers-in-c-journey-from.html.
And https://a5w1h.blogspot.com/2025/06/building-on-event-handlers-implementing.html.

```plantuml
@startuml

participant app as a1
participant message as msg
participant connector as con

a1 -> msg: send
msg -> msg: connect
msg -> msg: init
msg -> con: send_message

@enduml
```
