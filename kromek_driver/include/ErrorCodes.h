#ifndef DEVICEDEFINITIONS_H
#define DEVICEDEFINITIONS_H

enum ErrorCodes
{
    ERROR_OK = 0,

    // Data Interface errors
    ERROR_DEVICE_OPEN_FAILED,
    ERROR_READ_FAILED,
    ERROR_INTERNAL_DEVICE,
    ERROR_WRITE_FAILED,

    // Always keep at the end
    ERROR_END
};

#endif // DEVICEDEFINITIONS_H
