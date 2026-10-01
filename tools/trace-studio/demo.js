export function fictionalCycle() {
    const rows = [{ kind: 'session', v: 1, clock: 'active_us', heap_hooks: true }];
    let now = 1000, heap = 7000;
    const add = (kind, values = {}) => {
        now += 5000;
        rows.push({ kind, ts: now, addr: '0x0', old: '0x0', size: 0, gap: 14500,
            line: 0, name: '', file: '', ...values });
    };
    const object = (name, addr, owner = '0x0', port = -1, i2c = -1, ready = true) =>
        add('U', { name, addr, old: owner, line: port + 1, gap: i2c, file: ready ? 'ready' : 'unavailable' });
    const begin = (name, addr = '0x0') => add('B', { name, addr, size: heap,
        old: '0x640', file: 'fictional-example.cpp', line: 20 });
    const end = () => add('E', { size: heap, old: '0x640' });
    const checkpoint = name => add('C', { name, size: heap, line: 1600,
        addr: '0x2', old: '0x400' });
    add('I', { name: 'FICTIONAL Wisp cycle: demonstrates controls, not board evidence' });
    add('I', { name: 'Heap hooks enabled: allocations after capture are recorded' });
    object('Device manager', '0x20000100');
    object('Mux sensor controller', '0x20000200');
    object('MQTT publisher', '0x20000300');
    object('Analog A0 / battery A7', '0x20000500');
    object('SO2 / DFMultiGasSensor_0', '0x20003000', '0x20000200', 0, 0x74);
    object('CO / DFMultiGasSensor_1', '0x20003100', '0x20000200', 1, 0x74);
    object('O3 / DFMultiGasSensor_2', '0x20003200', '0x20000200', 2, 0x74);
    object('SEN66_5', '0x20004000', '0x20000200', 5, 0x6b);
    object('SHT31_6', '0x20004400', '0x20000200', 6, 0x44);
    object('TSL2591_7', '0x20004600', '0x20000200', 7, 0x29);
    object('STEMMA_7', '0x20004800', '0x20000200', 7, 0x36);
    object('AS7262_7', '0x20004900', '0x20000200', 7, 0x49);
    checkpoint('Capture baseline (fictional)');
    begin('void loop()');
    begin('void Manager::measure()', '0x20000100');
    begin('void Loom_Analog::measure()', '0x20000500'); end();
    begin('void Loom_Multiplexer::measure()', '0x20000200');
    for (const addr of ['0x20003000', '0x20003100', '0x20003200']) {
        begin('void Loom_DFMultiGasSensor::measure()', addr); end();
    }
    begin('void Loom_SEN66::measure()', '0x20004000'); end();
    begin('void Loom_TSL2591::measure()', '0x20004600'); end();
    begin('void Loom_STEMMA::measure()', '0x20004800'); end();
    begin('void Loom_AS7262::measure()', '0x20004900'); end();
    begin('void Loom_SHT31::measure()', '0x20004400');
    add('A', { addr: '0x20008000', size: 256, line: 0x1201 }); heap += 272;
    add('T', { addr: '0x20008000', name: 'Fictional temporary sensor buffer' });
    checkpoint('During sensor measurement (fictional)');
    add('F', { addr: '0x20008000', line: 0x1205 }); heap -= 272;
    end(); end(); end();
    checkpoint('After measuring sensors (fictional)');
    begin('void Loom_Multiplexer::refreshSensors()', '0x20000200');
    add('D', { addr: '0x20004400' }); add('F', { addr: '0x20004400' }); heap -= 128;
    begin('Module *Loom_Multiplexer::loadSensor()', '0x20000200');
    add('A', { addr: '0x20006000', size: 384, line: 0x1601 }); heap += 400;
    end(); object('SHT31_6', '0x20006000', '0x20000200', 6, 0x44, false);
    begin('void Loom_SHT31::initialize()', '0x20006000'); end();
    object('SHT31_6', '0x20006000', '0x20000200', 6, 0x44, true); end();
    checkpoint('After mux sensor refresh (fictional)');
    begin('void Manager::package()', '0x20000100'); end();
    checkpoint('After packaging sensor JSON (fictional)');
    begin('bool Loom_Hypnos::logToSD()'); begin('bool SDManager::log()');
    end(); end(); add('O', { size: 420 });
    checkpoint('After saving sample to SD (fictional)');
    begin('void Loom_MongoDB::publish()', '0x20000300');
    begin('bool MQTTComponent::publish()', '0x20000300');
    add('A', { addr: '0x20009000', size: 512, line: 0x2201 }); heap += 528;
    add('T', { addr: '0x20009000', name: 'Fictional MQTT packet buffer' });
    add('A', { addr: '0x2000a000', size: 128, line: 0x2205 }); heap += 144;
    add('T', { addr: '0x2000a000', name: 'Fictional retained connection state' });
    checkpoint('During MQTT upload (fictional)');
    add('F', { addr: '0x20009000', line: 0x2209 }); heap -= 528;
    end(); end(); checkpoint('After MQTT window (fictional)');
    add('V', { name: 'RTC requested sleep interval (fictional)', size: 30, gap: 0, file: 'seconds' });
    add('I', { name: 'Before standby: SD records saved' });
    begin('void Loom_Hypnos::sleep()'); end();
    add('V', { name: 'RTC elapsed at wake estimate (fictional)', size: 30, gap: 0, file: 'seconds' });
    add('V', { name: 'RTC wake within timing tolerance (fictional)', size: 1, gap: 0, file: 'boolean (1=yes)' });
    end(); add('O', { size: 510 });
    return rows.map(row => JSON.stringify(row)).join('\n') + '\n';
}
