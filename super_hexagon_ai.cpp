#include <Windows.h>
#include <cassert>
#include <algorithm>
#include <vector>
#include <type_traits>
#include <iostream>

struct superHexagonAPI {
    #pragma pack(push, 1)
        struct Wall {
            DWORD slot;
            DWORD distance;  
            BYTE enabled; 
            BYTE fill[3];
            DWORD unk2;
            DWORD unk3;
        };
    #pragma pack(pop)

    static_assert(sizeof(Wall) == 20, "Wall struct must be 0x14 bytes total");

    struct Offsets {
        enum : DWORD {
            BasePointer = 0x694B00,
            NumSlots = 0x1BC,
            NumWalls = 0x2930,
            FirstWall = 0x220,
            PlayerAngle = 0x2958,
            PlayerAngle2 = 0x2954,
            MouseDownLeft = 0x42858,
            MouseDownRight = 0x4285A,
            MouseDown = 0x42C45,
            WorldAngle = 0x1AC
        };
    };

    DWORD appBase;
    Memory const& memory;
    std::vector<Wall> walls;

    superHexagonAPI(Memory const& memory) : memory(memory) {
        appBase = memory.Read<DWORD>(Offsets::BasePointer);
        assert(appBase != 0);
    }

    DWORD getNumSlots() const {
        return memory.Read<DWORD>(appBase + Offsets::NumSlots);
    }

    DWORD getNumWalls() const {
        return memory.Read<DWORD>(appBase + Offsets::NumWalls);
    }

    void updateWalls() {
        walls.clear();
        auto const numWalls = getNumWalls();
        walls.resize(numWalls);
        memory.readBytes(appBase + Offsets::FirstWall, walls.data(), sizeof(Wall) * numWalls);
    }

    DWORD getPlayerAngle() const {
        return memory.Read<DWORD>(appBase + Offsets::PlayerAngle);
    }

    DWORD getPlayerSlot() const {
        float const angle = static_cast<float>(getPlayerAngle());
        return static_cast<DWORD>(angle / 360.0f * getNumSlots());
    }

    void setPlayerSlot(DWORD slot) const {
        DWORD const angle = 360 / getNumSlots() * (slot % getNumSlots()) + (180 / getNumSlots());
        memory.Write(appBase + Offsets::PlayerAngle, angle);
        memory.Write(appBase + Offsets::PlayerAngle2, angle);
    }

    void startMovingLeft() const {
        memory.Write<BYTE>(appBase + Offsets::MouseDownLeft, 1);
        memory.Write<BYTE>(appBase + Offsets::MouseDown, 1);
    }

    void startMovingRight() const {
        memory.Write<BYTE>(appBase + Offsets::MouseDownRight, 1);
        memory.Write<BYTE>(appBase + Offsets::MouseDown, 1);
    }

    void releaseMouse() const {
        memory.Write<BYTE>(appBase + Offsets::MouseDownLeft, 0);
        memory.Write<BYTE>(appBase + Offsets::MouseDownRight, 0);
        memory.Write<BYTE>(appBase + Offsets::MouseDown, 0);
    }

    DWORD getWorldAngle() const {
        return memory.Read<DWORD>(appBase + Offsets::WorldAngle);
    }

    void setWorldAngle(DWORD angle) const {
        memory.Write<DWORD>(appBase + Offsets::WorldAngle, angle);
    }
};


struct Memory {
    HANDLE const mProcess;
    Memory(HANDLE const mProcess) : mProcess(mProcess) { }

    ~Memory() {
        if (mProcess)
            CloseHandle(mProcess);
    }

    template <typename T>
    inline T Read(DWORD address) const {
        static_assert(std::is_pod<T>::value, "");

        T data = {0};
        SIZE_T numRead = -1;
        auto success = ReadProcessMemory(mProcess, 
        	reinterpret_cast<LPCVOID>(address), 
            &data, sizeof(T), &numRead);

        assert(success && numRead == sizeof(T));
        
        return data;
    }

    template <typename T>
    inline T& Read(DWORD address, T& data) const {
        static_assert(std::is_pod<T>::value, "");

        SIZE_T numRead = -1;
        auto success = ReadProcessMemory(mProcess, 
        	reinterpret_cast<LPCVOID>(address), 
            &data, sizeof(T), &numRead);

        assert(success && numRead == sizeof(T));

        return data;
    }

    template <typename T>
    inline void Write(DWORD address, T data) const {
        static_assert(std::is_pod<T>::value, "");

        SIZE_T numWritten = -1;
        auto success = WriteProcessMemory(mProcess, 
        	reinterpret_cast<LPVOID>(address),
            &data, sizeof(T), &numWritten);

        assert(success && numWritten == sizeof(T));
    }

    void readBytes(DWORD address, void* buffer, SIZE_T length) const {
        SIZE_T numRead = -1;
        auto success = ReadProcessMemory(mProcess, reinterpret_cast<LPCVOID>(address),
            buffer, length, &numRead);

        assert(success && numRead == length);
    }
};


int main(int argc, char** argv, char** env) {
    auto hWnd = FindWindow(nullptr, L"Super Hexagon");
    assert(hWnd);

    DWORD processId = -1;
    GetWindowThreadProcessId(hWnd, &processId);
    assert(processId > 0);

    auto const mProcess = OpenProcess(
        PROCESS_VM_READ |
        PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION,
        FALSE, processId);

    assert(mProcess);

    Memory const memory(mProcess);
    superHexagonAPI api(memory);

    for (;;) {
        api.updateWalls();

        if (!api.walls.empty()) {
            auto const numSlots = api.getNumSlots();
            std::vector<DWORD> minDistances(numSlots, -1);

            std::for_each(api.walls.begin(), api.walls.end(), [&] (superHexagonAPI::Wall const& a) {
                if (a.distance > 0 && a.enabled) {
                    minDistances[a.slot % numSlots] = min(minDistances[a.slot % numSlots], a.distance);
                }
            });

            auto const maxElement = std::max_element(minDistances.begin(), minDistances.end());
            DWORD const targetSlot = static_cast<DWORD>(std::distance(minDistances.begin(), maxElement));
            std::cout << "Moving to slot [" << targetSlot << "]; world angle is: " << api.getWorldAngle() << ".\n";

            api.setPlayerSlot(targetSlot);
        }

        Sleep(10);
        system("cls");
    }

    return 0;
}
