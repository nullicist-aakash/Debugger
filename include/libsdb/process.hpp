#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sys/types.h>
#include <libsdb/registers.hpp>

namespace sdb {
    /**
     * Possible states for the process.
     */
    enum class process_state {
        /**
         * If the process was stopped by a signal.
         */
        STOPPED,
        /**
         * If the process is running.
         */
        RUNNING,
        /**
         * If the process exited on its own.
         */
        EXITED,
        /**
         * If the process was terminated by an uncaught external signal.
         */
        TERMINATED
    };

    /**
     * Holds the information for process stop and some information about the stop.
     */
    struct stop_reason {
        /**
         * Constructor.
         * @param wait_status The status returned by the call to `waitpid`.
         */
        explicit stop_reason(int wait_status);

        /**
         * The reason for process stop.
         */
        process_state reason;
        /**
         * If the reason is that child exited normally, this contains the exit value.
         * Otherwise, this contains the signal which caused the child to stop/terminate.
         */
        std::uint8_t info;
    };


    class process {
    public:
        /**
         * Don't allow users to explicitly create a `process` type.
         */
        process() = delete;
        /**
         * It doesn't make sense to copy a process.
         */
        process(const process&) = delete;
        /**
         * It doesn't make sense to copy a process.
         */
        process& operator=(const process&) = delete;

        /**
         * Helps in the launch of a new process based on the program path and no arguments. When the method returns,
         * the process will not be running iff debug = true.
         * @param path The path to the program that we want to launch.
         * @param debug If true, we will also attach to the child process. Otherwise, nothing special is done to the child process.
         * @return The instance of `process` class.
         */
        static std::unique_ptr<process> launch(const std::filesystem::path& path, bool debug = true);

        /**
         * Helps in the attachment of debugger to already running process. When the method returns,
         * the process will not be running
         * @param pid PID of the process which we want to attach to.
         * @return The `process` instance for the PID if we attached to it.
         */
        static std::unique_ptr<process> attach(pid_t pid);

        /**
         * Waits for the process to stop/pause its execution.
         * @return Returns the reason for the process to pause/stop.
         */
        stop_reason wait_on_signal();

        /**
         * Forcefully resumes the process.
         */
        void resume();

        /**
         * Fetches the set of registers associated with the current process.
         * @param self Instance of self.
         * @return The set of registers associated with current process.
         */
        registers& get_registers(this auto&& self) { return self.m_registers; }

        /**
         * Writes 8 byte data to specified user's struct offset.
         * @param offset user struct's offset to write the data to.
         * @param data The data to write.
         */
        void write_user_struct(std::size_t offset, std::uint64_t data);

        /**
         * Bulk writer for general purpose registers.
         * @param gprs General purpose registers to bulk write.
         */
        void write_gprs(const user_regs_struct& gprs);

        /**
         * Bilk writer for floating point registers.
         * @param fprs Floating points registers to bulk write.
         */
        void write_fprs(const user_fpregs_struct& fprs);

        /**
         * Fetches the PID of the process.
         * @return PID of the process.
         */
        [[nodiscard]] auto pid() const { return m_pid; }

        /**
         * Destructor.
         */
        ~process();
    private:

        /**
         * Private constructor to be used by static builders.
         * @param pid PID of the process.
         * @param terminate_on_end If true, we will terminate the process in self's destructor.
         * @param is_attached If true, it means that we are attached to the process.
         */
        process(pid_t pid, bool terminate_on_end, bool is_attached) :
            m_pid(pid), m_terminate_on_end(terminate_on_end), m_is_attached(is_attached),
            m_registers(new registers(*this)) {

        }

        /**
         * Helper method to update local copy of all the registers of a process.
         */
        void read_all_registers();

        /**
         * PID of the process.
         */
        const pid_t m_pid;
        /**
         * If true, we will terminate the process with PID `m_pid` in destructor.
         */
        const bool m_terminate_on_end;
        /**
         * If true, it means that we are attached to the process.
         */
        const bool m_is_attached;
        /**
         * Represents the current state of the process.
         */
        process_state m_state = process_state::STOPPED;
        /**
         * Process specific registers.
         */
        std::unique_ptr<registers> m_registers;
    };
}
