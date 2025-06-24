#pragma once 

namespace EXECUTOR {

    class Task {
    public:
        virtual ~Task() = default;
        
        // Pure virtual function must be overridden
        virtual void execute(int worker_id) = 0;
    }; 

} // namespace EXECUTOR