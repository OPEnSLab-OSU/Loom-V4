#pragma once

class Primative{
    public:

        /**
         * Set it to use float data
         */ 
        template<typename T>
        void setData(T data){
           stringData = String(data);
        };

        /**
         * Get the float data
         */ 
        String getData(){
            return stringData;
        };

    private:
        String stringData;

};