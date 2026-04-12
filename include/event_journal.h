#pragma once
#include "events.h"
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

class EventJournal {
private:
    int fd;
    size_t file_size;
    size_t offset;
    OutboundEvent* mapped_buffer;

public:
    EventJournal(const std::string& filepath, size_t max_events = 10000000) 
        : offset(0) {
        
        file_size = max_events * sizeof(OutboundEvent);
        
        fd = open(filepath.c_str(), O_RDWR | O_CREAT, 0666);
        if (fd == -1) {
            throw std::runtime_error("Failed to open journal file");
        }

        // Pre-allocate file size
        if (ftruncate(fd, file_size) == -1) {
            close(fd);
            throw std::runtime_error("Failed to truncate journal file");
        }

        mapped_buffer = (OutboundEvent*)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapped_buffer == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to mmap journal file");
        }
    }

    ~EventJournal() {
        if (mapped_buffer != MAP_FAILED) {
            munmap(mapped_buffer, file_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    // O(1) Zero-Copy append
    void append(const OutboundEvent& ev) {
        if (offset < file_size / sizeof(OutboundEvent)) {
            mapped_buffer[offset++] = ev;
        } else {
            std::cerr << "Journal is full!" << std::endl;
        }
    }
};
