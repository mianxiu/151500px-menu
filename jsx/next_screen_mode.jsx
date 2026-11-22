(function () {
    var id = stringIDToTypeID;
    var commands = [5991, 5992, 5993];
    function current() {
        var selected = -1;
        for (var i = 0; i < commands.length; i++) {
            var ref = new ActionReference(), query = new ActionDescriptor();
            ref.putEnumerated(id("application"), id("ordinal"), id("targetEnum"));
            query.putReference(id("null"), ref);
            query.putString(id("command"), "getCommandEnabled");
            query.putInteger(id("commandID"), commands[i]);
            var state = executeAction(id("uiInfo"), query, DialogModes.NO).getObjectValue(id("result"));
            if (state.getInteger(id("commandID")) !== commands[i]) throw Error("无法确认屏幕模式命令");
            if (state.getBoolean(id("checked"))) {
                if (selected !== -1) throw Error("屏幕模式状态不一致");
                selected = i;
            }
        }
        if (selected === -1) throw Error("无法确认当前屏幕模式");
        return selected;
    }
    return commands[(current() + 1) % commands.length];
}());
