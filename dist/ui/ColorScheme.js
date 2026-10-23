/**
 * BendCore Color Scheme (11 Categories)
 * Official specification: BENDCORE_SPECIFICATION_FINAL_LOCKED.md
 * Reference Interface: Assembly-Color.dc.html (turn 312)
 */
export const colorScheme = {
    tube: {
        name: "Tube",
        hex: "#1F6FC5",
        usage: "Tube bodies, main elements",
        examples: ["New Tube", "Finished Tube Length", "Tube properties"],
    },
    create: {
        name: "Create",
        hex: "#2E9E5B",
        usage: "Creation commands (objects, geometry)",
        examples: ["New Sketch", "New Tube", "New Component", "Insert Port"],
    },
    delete: {
        name: "Delete",
        hex: "#D93A3A",
        usage: "Delete, prohibition, errors, reset",
        examples: ["Delete Element", "Clear", "Reset to Default", "Remove Link"],
    },
    import: {
        name: "Import",
        hex: "#0E97A3",
        usage: "Imported data, mesh, external geometry",
        examples: ["Import STEP", "Import Geometry", "Load Assembly", "Recognize"],
    },
    links: {
        name: "Links",
        hex: "#8A4FD8",
        usage: "Environment references, constraints (same as sketch)",
        examples: ["Port", "Support", "Trace", "Link", "Reference Plane"],
    },
    edit: {
        name: "Edit",
        hex: "#E8820C",
        usage: "Parameter change, movement, transformation",
        examples: ["Edit Tube", "Move Component", "Update Parameters", "Modify"],
    },
    update: {
        name: "Update",
        hex: "#F5B517",
        usage: "Recalculate, synchronize, refresh",
        examples: ["Refresh", "Update Import", "Recalculate", "Sync Links"],
    },
    freeze: {
        name: "Freeze",
        hex: "#2D9CDB",
        usage: "Lock, freeze state, prevent changes",
        examples: ["Freeze Link", "Lock Parameter", "Suppress", "Disable"],
    },
    auxiliary: {
        name: "Auxiliary",
        hex: "#C9920A",
        usage: "Help geometry, construction aids",
        examples: [
            "Coordinate System",
            "Reference Plane",
            "Axis",
            "Point",
            "Guide Line",
        ],
    },
    axes_x: {
        name: "X Axis",
        hex: "#FF0000",
        usage: "X-axis of coordinate system",
        examples: ["X Direction", "X Constraint"],
    },
    axes_y: {
        name: "Y Axis",
        hex: "#00FF00",
        usage: "Y-axis of coordinate system",
        examples: ["Y Direction", "Y Constraint"],
    },
    axes_z: {
        name: "Z Axis",
        hex: "#0000FF",
        usage: "Z-axis of coordinate system",
        examples: ["Z Direction", "Z Constraint"],
    },
    neutral: {
        name: "Neutral",
        hex: "#757575",
        usage: "Outlines, shadows, neutral details",
        examples: ["Contour", "Background", "Border", "Default"],
    },
};
export const getColorHex = (category) => {
    const cat = colorScheme[category];
    return cat ? cat.hex : colorScheme.neutral.hex;
};
export const getCategoryByUsage = (usage) => {
    return Object.values(colorScheme).find((cat) => cat.usage.toLowerCase().includes(usage.toLowerCase()));
};
export const listAllCategories = () => {
    return Object.values(colorScheme);
};
//# sourceMappingURL=ColorScheme.js.map