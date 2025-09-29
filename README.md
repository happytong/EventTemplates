# EventTemplates
You may find the details of the description of these 4 classes in https://a5w1h.blogspot.com/2025/06/event-handlers-in-c-journey-from.html.
And https://a5w1h.blogspot.com/2025/06/building-on-event-handlers-implementing.html.

#testing for flowchart generating
```mermaid
flowchart TD
    Start([Start]) --> A[Enter credentials]
    A --> B{Valid?}
    B -->|Yes| C[Show dashboard]
    B -->|No| D[Show error]
    C --> End([End])
    D --> A
```
```mermaid
classDiagram
    class User {
        +String name
        +login()
        +logout()
    }

    class Account {
        +double balance
        +deposit(amount)
        +withdraw(amount)
    }

    User --> Account : owns
```
```mermaid
sequenceDiagram
    participant User
    participant AuthService
    participant Database

    User->>AuthService: login(username, password)
    AuthService->>Database: validateUser()
    Database-->>AuthService: valid / invalid
    AuthService-->>User: login result
```
```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Active : user logs in
    Active --> Idle : user logs out
    Active --> Locked : timeout
    Locked --> Active : correct password
    Locked --> Idle : too many failures
```
