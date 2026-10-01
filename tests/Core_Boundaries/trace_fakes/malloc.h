#pragma once
struct mallinfo {
    int uordblks;
    int fordblks;
    int ordblks;
    int keepcost;
};
inline struct mallinfo mallinfo() { return {3000, 512, 4, 256}; }
