local status, err = pcall(function()

local ffi =require("ffi")

local mylib=ffi.load("mylib.dll")
ffi.cdef[[
void open_usb(void);
void close_usb(void);
void write_usb(unsigned char* arr);
void write_usb(unsigned char* arr);
unsigned char* read_usb(void);
void free_array(unsigned char* arr);
]]

mylib.open_usb()

local my_array = {}
for i = 1, 64 do
  my_array[i] = i -- change this to use a different byte value
end
local arr_size = #my_array
print(arr_size)
--load the array into a C array
local arr_c = ffi.new("unsigned char[?]", arr_size)
for i = 1, arr_size do
  arr_c[i - 1] = my_array[i]
end

mylib.write_usb(arr_c)

local size=64
local arr_c = mylib.read_usb()
-- convert C array to Lua array
local arr_lua = {}
for i = 1, size do
  arr_lua[i] = arr_c[i - 1]
end

-- print lua array
for i, v in ipairs(arr_lua) do
  print(i, v)
end
mylib.free_array(arr_c)
mylib.close_usb()
end)
if not status then
print(err);
end