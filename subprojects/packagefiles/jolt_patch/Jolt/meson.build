sources = files(
  'AABBTree/AABBTreeBuilder.cpp',
  'AABBTree/AABBTreeBuilder.h',
  'AABBTree/AABBTreeToBuffer.h',
  'AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h',
  'AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h',
  'ConfigurationString.h',
  'Core/ARMNeon.h',
  'Core/Array.h',
  'Core/Atomics.h',
  'Core/BinaryHeap.h',
  'Core/ByteBuffer.h',
  'Core/Color.cpp',
  'Core/Color.h',
  'Core/Core.h',
  'Core/Factory.cpp',
  'Core/Factory.h',
  'Core/FixedSizeFreeList.h',
  'Core/FPControlWord.h',
  'Core/FPException.h',
  'Core/FPFlushDenormals.h',
  'Core/HashCombine.h',
  'Core/HashTable.h',
  'Core/InsertionSort.h',
  'Core/IssueReporting.cpp',
  'Core/IssueReporting.h',
  'Core/JobSystem.h',
  'Core/JobSystemSingleThreaded.cpp',
  'Core/JobSystemSingleThreaded.h',
  'Core/JobSystemThreadPool.cpp',
  'Core/JobSystemThreadPool.h',
  'Core/JobSystemWithBarrier.cpp',
  'Core/JobSystemWithBarrier.h',
  'Core/LinearCurve.cpp',
  'Core/LinearCurve.h',
  'Core/LockFreeHashMap.h',
  'Core/Memory.cpp',
  'Core/Memory.h',
  'Core/Mutex.h',
  'Core/MutexArray.h',
  'Core/NonCopyable.h',
  'Core/Profiler.cpp',
  'Core/Profiler.h',
  'Core/QuickSort.h',
  'Core/Reference.h',
  'Core/Result.h',
  'Core/RTTI.cpp',
  'Core/RTTI.h',
  'Core/ScopeExit.h',
  'Core/Semaphore.cpp',
  'Core/Semaphore.h',
  'Core/StaticArray.h',
  'Core/STLAlignedAllocator.h',
  'Core/STLAllocator.h',
  'Core/STLLocalAllocator.h',
  'Core/STLTempAllocator.h',
  'Core/StreamIn.h',
  'Core/StreamOut.h',
  'Core/StreamUtils.h',
  'Core/StreamWrapper.h',
  'Core/StridedPtr.h',
  'Core/StringTools.cpp',
  'Core/StringTools.h',
  'Core/TempAllocator.h',
  'Core/TickCounter.cpp',
  'Core/TickCounter.h',
  'Core/UnorderedMap.h',
  'Core/UnorderedSet.h',
  'Geometry/AABox.h',
  'Geometry/AABox4.h',
  'Geometry/ClipPoly.h',
  'Geometry/ClosestPoint.h',
  'Geometry/ConvexHullBuilder.cpp',
  'Geometry/ConvexHullBuilder.h',
  'Geometry/ConvexHullBuilder2D.cpp',
  'Geometry/ConvexHullBuilder2D.h',
  'Geometry/ConvexSupport.h',
  'Geometry/Ellipse.h',
  'Geometry/EPAConvexHullBuilder.h',
  'Geometry/EPAPenetrationDepth.h',
  'Geometry/GJKClosestPoint.h',
  'Geometry/IndexedTriangle.h',
  'Geometry/Indexify.cpp',
  'Geometry/Indexify.h',
  'Geometry/MortonCode.h',
  'Geometry/OrientedBox.cpp',
  'Geometry/OrientedBox.h',
  'Geometry/Plane.h',
  'Geometry/RayAABox.h',
  'Geometry/RayCapsule.h',
  'Geometry/RayCylinder.h',
  'Geometry/RaySphere.h',
  'Geometry/RayTriangle.h',
  'Geometry/Sphere.h',
  'Geometry/Triangle.h',
  'Jolt.h',
  'Math/BVec16.h',
  'Math/DMat44.h',
  'Math/Double3.h',
  'Math/DVec3.h',
  'Math/DynMatrix.h',
  'Math/EigenValueSymmetric.h',
  'Math/FindRoot.h',
  'Math/Float2.h',
  'Math/Float3.h',
  'Math/Float4.h',
  'Math/GaussianElimination.h',
  'Math/HalfFloat.h',
  'Math/Mat44.h',
  'Math/Math.h',
  'Math/MathTypes.h',
  'Math/Matrix.h',
  'Math/Quat.h',
  'Math/Real.h',
  'Math/Swizzle.h',
  'Math/Trigonometry.h',
  'Math/UVec4.h',
  'Math/Vec3.cpp',
  'Math/Vec3.h',
  'Math/Vec4.h',
  'Math/Vector.h',
  'ObjectStream/ObjectStream.h',
  'ObjectStream/SerializableAttribute.h',
  'ObjectStream/SerializableAttributeEnum.h',
  'ObjectStream/SerializableAttributeTyped.h',
  'ObjectStream/SerializableObject.cpp',
  'ObjectStream/SerializableObject.h',
  'ObjectStream/TypeDeclarations.h',
  'Physics/Body/AllowedDOFs.h',
  'Physics/Body/Body.cpp',
  'Physics/Body/Body.h',
  'Physics/Body/BodyAccess.h',
  'Physics/Body/BodyActivationListener.h',
  'Physics/Body/BodyCreationSettings.cpp',
  'Physics/Body/BodyCreationSettings.h',
  'Physics/Body/BodyFilter.h',
  'Physics/Body/BodyID.h',
  'Physics/Body/BodyInterface.cpp',
  'Physics/Body/BodyInterface.h',
  'Physics/Body/BodyLock.h',
  'Physics/Body/BodyLockInterface.h',
  'Physics/Body/BodyLockMulti.h',
  'Physics/Body/BodyManager.cpp',
  'Physics/Body/BodyManager.h',
  'Physics/Body/BodyPair.h',
  'Physics/Body/BodyType.h',
  'Physics/Body/MassProperties.cpp',
  'Physics/Body/MassProperties.h',
  'Physics/Body/MotionProperties.cpp',
  'Physics/Body/MotionProperties.h',
  'Physics/Body/MotionQuality.h',
  'Physics/Body/MotionType.h',
  'Physics/Character/Character.cpp',
  'Physics/Character/Character.h',
  'Physics/Character/CharacterBase.cpp',
  'Physics/Character/CharacterBase.h',
  'Physics/Character/CharacterID.h',
  'Physics/Character/CharacterVirtual.cpp',
  'Physics/Character/CharacterVirtual.h',
  'Physics/Collision/AABoxCast.h',
  'Physics/Collision/ActiveEdgeMode.h',
  'Physics/Collision/ActiveEdges.h',
  'Physics/Collision/BackFaceMode.h',
  'Physics/Collision/BroadPhase/BroadPhase.cpp',
  'Physics/Collision/BroadPhase/BroadPhase.h',
  'Physics/Collision/BroadPhase/BroadPhaseBruteForce.cpp',
  'Physics/Collision/BroadPhase/BroadPhaseBruteForce.h',
  'Physics/Collision/BroadPhase/BroadPhaseLayer.h',
  'Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h',
  'Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h',
  'Physics/Collision/BroadPhase/BroadPhaseQuadTree.cpp',
  'Physics/Collision/BroadPhase/BroadPhaseQuadTree.h',
  'Physics/Collision/BroadPhase/BroadPhaseQuery.h',
  'Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterMask.h',
  'Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h',
  'Physics/Collision/BroadPhase/QuadTree.cpp',
  'Physics/Collision/BroadPhase/QuadTree.h',
  'Physics/Collision/CastConvexVsTriangles.cpp',
  'Physics/Collision/CastConvexVsTriangles.h',
  'Physics/Collision/CastResult.h',
  'Physics/Collision/CastSphereVsTriangles.cpp',
  'Physics/Collision/CastSphereVsTriangles.h',
  'Physics/Collision/CollectFacesMode.h',
  'Physics/Collision/CollideConvexVsTriangles.cpp',
  'Physics/Collision/CollideConvexVsTriangles.h',
  'Physics/Collision/CollidePointResult.h',
  'Physics/Collision/CollideShape.h',
  'Physics/Collision/CollideShapeVsShapePerLeaf.h',
  'Physics/Collision/CollideSoftBodyVertexIterator.h',
  'Physics/Collision/CollideSoftBodyVerticesVsTriangles.h',
  'Physics/Collision/CollideSphereVsTriangles.cpp',
  'Physics/Collision/CollideSphereVsTriangles.h',
  'Physics/Collision/CollisionCollector.h',
  'Physics/Collision/CollisionCollectorImpl.h',
  'Physics/Collision/CollisionDispatch.cpp',
  'Physics/Collision/CollisionDispatch.h',
  'Physics/Collision/CollisionGroup.cpp',
  'Physics/Collision/CollisionGroup.h',
  'Physics/Collision/ContactListener.h',
  'Physics/Collision/EstimateCollisionResponse.cpp',
  'Physics/Collision/EstimateCollisionResponse.h',
  'Physics/Collision/GroupFilter.cpp',
  'Physics/Collision/GroupFilter.h',
  'Physics/Collision/GroupFilterTable.cpp',
  'Physics/Collision/GroupFilterTable.h',
  'Physics/Collision/InternalEdgeRemovingCollector.h',
  'Physics/Collision/ManifoldBetweenTwoFaces.cpp',
  'Physics/Collision/ManifoldBetweenTwoFaces.h',
  'Physics/Collision/NarrowPhaseQuery.cpp',
  'Physics/Collision/NarrowPhaseQuery.h',
  'Physics/Collision/NarrowPhaseStats.cpp',
  'Physics/Collision/NarrowPhaseStats.h',
  'Physics/Collision/ObjectLayer.h',
  'Physics/Collision/ObjectLayerPairFilterMask.h',
  'Physics/Collision/ObjectLayerPairFilterTable.h',
  'Physics/Collision/PhysicsMaterial.cpp',
  'Physics/Collision/PhysicsMaterial.h',
  'Physics/Collision/PhysicsMaterialSimple.cpp',
  'Physics/Collision/PhysicsMaterialSimple.h',
  'Physics/Collision/RayCast.h',
  'Physics/Collision/Shape/BoxShape.cpp',
  'Physics/Collision/Shape/BoxShape.h',
  'Physics/Collision/Shape/CapsuleShape.cpp',
  'Physics/Collision/Shape/CapsuleShape.h',
  'Physics/Collision/Shape/CompoundShape.cpp',
  'Physics/Collision/Shape/CompoundShape.h',
  'Physics/Collision/Shape/CompoundShapeVisitors.h',
  'Physics/Collision/Shape/ConvexHullShape.cpp',
  'Physics/Collision/Shape/ConvexHullShape.h',
  'Physics/Collision/Shape/ConvexShape.cpp',
  'Physics/Collision/Shape/ConvexShape.h',
  'Physics/Collision/Shape/CylinderShape.cpp',
  'Physics/Collision/Shape/CylinderShape.h',
  'Physics/Collision/Shape/DecoratedShape.cpp',
  'Physics/Collision/Shape/DecoratedShape.h',
  'Physics/Collision/Shape/EmptyShape.cpp',
  'Physics/Collision/Shape/EmptyShape.h',
  'Physics/Collision/Shape/GetTrianglesContext.h',
  'Physics/Collision/Shape/HeightFieldShape.cpp',
  'Physics/Collision/Shape/HeightFieldShape.h',
  'Physics/Collision/Shape/MeshShape.cpp',
  'Physics/Collision/Shape/MeshShape.h',
  'Physics/Collision/Shape/MutableCompoundShape.cpp',
  'Physics/Collision/Shape/MutableCompoundShape.h',
  'Physics/Collision/Shape/OffsetCenterOfMassShape.cpp',
  'Physics/Collision/Shape/OffsetCenterOfMassShape.h',
  'Physics/Collision/Shape/PlaneShape.cpp',
  'Physics/Collision/Shape/PlaneShape.h',
  'Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h',
  'Physics/Collision/Shape/RotatedTranslatedShape.cpp',
  'Physics/Collision/Shape/RotatedTranslatedShape.h',
  'Physics/Collision/Shape/ScaledShape.cpp',
  'Physics/Collision/Shape/ScaledShape.h',
  'Physics/Collision/Shape/ScaleHelpers.h',
  'Physics/Collision/Shape/Shape.cpp',
  'Physics/Collision/Shape/Shape.h',
  'Physics/Collision/Shape/SphereShape.cpp',
  'Physics/Collision/Shape/SphereShape.h',
  'Physics/Collision/Shape/StaticCompoundShape.cpp',
  'Physics/Collision/Shape/StaticCompoundShape.h',
  'Physics/Collision/Shape/SubShapeID.h',
  'Physics/Collision/Shape/SubShapeIDPair.h',
  'Physics/Collision/Shape/TaperedCapsuleShape.cpp',
  'Physics/Collision/Shape/TaperedCapsuleShape.h',
  'Physics/Collision/Shape/TaperedCylinderShape.cpp',
  'Physics/Collision/Shape/TaperedCylinderShape.h',
  'Physics/Collision/Shape/TriangleShape.cpp',
  'Physics/Collision/Shape/TriangleShape.h',
  'Physics/Collision/ShapeCast.h',
  'Physics/Collision/ShapeFilter.h',
  'Physics/Collision/SimShapeFilter.h',
  'Physics/Collision/SimShapeFilterWrapper.h',
  'Physics/Collision/SortReverseAndStore.h',
  'Physics/Collision/TransformedShape.cpp',
  'Physics/Collision/TransformedShape.h',
  'Physics/Constraints/CalculateSolverSteps.h',
  'Physics/Constraints/ConeConstraint.cpp',
  'Physics/Constraints/ConeConstraint.h',
  'Physics/Constraints/Constraint.cpp',
  'Physics/Constraints/Constraint.h',
  'Physics/Constraints/ConstraintManager.cpp',
  'Physics/Constraints/ConstraintManager.h',
  'Physics/Constraints/ConstraintPart/AngleConstraintPart.h',
  'Physics/Constraints/ConstraintPart/AxisConstraintPart.h',
  'Physics/Constraints/ConstraintPart/DualAxisConstraintPart.h',
  'Physics/Constraints/ConstraintPart/GearConstraintPart.h',
  'Physics/Constraints/ConstraintPart/HingeRotationConstraintPart.h',
  'Physics/Constraints/ConstraintPart/IndependentAxisConstraintPart.h',
  'Physics/Constraints/ConstraintPart/PointConstraintPart.h',
  'Physics/Constraints/ConstraintPart/RackAndPinionConstraintPart.h',
  'Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h',
  'Physics/Constraints/ConstraintPart/RotationQuatConstraintPart.h',
  'Physics/Constraints/ConstraintPart/SpringPart.h',
  'Physics/Constraints/ConstraintPart/SwingTwistConstraintPart.h',
  'Physics/Constraints/ContactConstraintManager.cpp',
  'Physics/Constraints/ContactConstraintManager.h',
  'Physics/Constraints/DistanceConstraint.cpp',
  'Physics/Constraints/DistanceConstraint.h',
  'Physics/Constraints/FixedConstraint.cpp',
  'Physics/Constraints/FixedConstraint.h',
  'Physics/Constraints/GearConstraint.cpp',
  'Physics/Constraints/GearConstraint.h',
  'Physics/Constraints/HingeConstraint.cpp',
  'Physics/Constraints/HingeConstraint.h',
  'Physics/Constraints/MotorSettings.cpp',
  'Physics/Constraints/MotorSettings.h',
  'Physics/Constraints/PathConstraint.cpp',
  'Physics/Constraints/PathConstraint.h',
  'Physics/Constraints/PathConstraintPath.cpp',
  'Physics/Constraints/PathConstraintPath.h',
  'Physics/Constraints/PathConstraintPathHermite.cpp',
  'Physics/Constraints/PathConstraintPathHermite.h',
  'Physics/Constraints/PointConstraint.cpp',
  'Physics/Constraints/PointConstraint.h',
  'Physics/Constraints/PulleyConstraint.cpp',
  'Physics/Constraints/PulleyConstraint.h',
  'Physics/Constraints/RackAndPinionConstraint.cpp',
  'Physics/Constraints/RackAndPinionConstraint.h',
  'Physics/Constraints/SixDOFConstraint.cpp',
  'Physics/Constraints/SixDOFConstraint.h',
  'Physics/Constraints/SliderConstraint.cpp',
  'Physics/Constraints/SliderConstraint.h',
  'Physics/Constraints/SpringSettings.cpp',
  'Physics/Constraints/SpringSettings.h',
  'Physics/Constraints/SwingTwistConstraint.cpp',
  'Physics/Constraints/SwingTwistConstraint.h',
  'Physics/Constraints/TwoBodyConstraint.cpp',
  'Physics/Constraints/TwoBodyConstraint.h',
  'Physics/DeterminismLog.cpp',
  'Physics/DeterminismLog.h',
  'Physics/EActivation.h',
  'Physics/EPhysicsUpdateError.h',
  'Physics/IslandBuilder.cpp',
  'Physics/IslandBuilder.h',
  'Physics/LargeIslandSplitter.cpp',
  'Physics/LargeIslandSplitter.h',
  'Physics/PhysicsLock.h',
  'Physics/PhysicsScene.cpp',
  'Physics/PhysicsScene.h',
  'Physics/PhysicsSettings.h',
  'Physics/PhysicsStepListener.h',
  'Physics/PhysicsSystem.cpp',
  'Physics/PhysicsSystem.h',
  'Physics/PhysicsUpdateContext.cpp',
  'Physics/PhysicsUpdateContext.h',
  'Physics/Ragdoll/Ragdoll.cpp',
  'Physics/Ragdoll/Ragdoll.h',
  'Physics/SoftBody/SoftBodyContactListener.h',
  'Physics/SoftBody/SoftBodyCreationSettings.cpp',
  'Physics/SoftBody/SoftBodyCreationSettings.h',
  'Physics/SoftBody/SoftBodyManifold.h',
  'Physics/SoftBody/SoftBodyMotionProperties.cpp',
  'Physics/SoftBody/SoftBodyMotionProperties.h',
  'Physics/SoftBody/SoftBodyShape.cpp',
  'Physics/SoftBody/SoftBodyShape.h',
  'Physics/SoftBody/SoftBodySharedSettings.cpp',
  'Physics/SoftBody/SoftBodySharedSettings.h',
  'Physics/SoftBody/SoftBodyUpdateContext.h',
  'Physics/SoftBody/SoftBodyVertex.h',
  'Physics/StateRecorder.h',
  'Physics/StateRecorderImpl.cpp',
  'Physics/StateRecorderImpl.h',
  'Physics/Vehicle/MotorcycleController.cpp',
  'Physics/Vehicle/MotorcycleController.h',
  'Physics/Vehicle/TrackedVehicleController.cpp',
  'Physics/Vehicle/TrackedVehicleController.h',
  'Physics/Vehicle/VehicleAntiRollBar.cpp',
  'Physics/Vehicle/VehicleAntiRollBar.h',
  'Physics/Vehicle/VehicleCollisionTester.cpp',
  'Physics/Vehicle/VehicleCollisionTester.h',
  'Physics/Vehicle/VehicleConstraint.cpp',
  'Physics/Vehicle/VehicleConstraint.h',
  'Physics/Vehicle/VehicleController.cpp',
  'Physics/Vehicle/VehicleController.h',
  'Physics/Vehicle/VehicleDifferential.cpp',
  'Physics/Vehicle/VehicleDifferential.h',
  'Physics/Vehicle/VehicleEngine.cpp',
  'Physics/Vehicle/VehicleEngine.h',
  'Physics/Vehicle/VehicleTrack.cpp',
  'Physics/Vehicle/VehicleTrack.h',
  'Physics/Vehicle/VehicleTransmission.cpp',
  'Physics/Vehicle/VehicleTransmission.h',
  'Physics/Vehicle/Wheel.cpp',
  'Physics/Vehicle/Wheel.h',
  'Physics/Vehicle/WheeledVehicleController.cpp',
  'Physics/Vehicle/WheeledVehicleController.h',
  'RegisterTypes.cpp',
  'RegisterTypes.h',
  'Renderer/DebugRenderer.cpp',
  'Renderer/DebugRenderer.h',
  'Renderer/DebugRendererPlayback.cpp',
  'Renderer/DebugRendererPlayback.h',
  'Renderer/DebugRendererRecorder.cpp',
  'Renderer/DebugRendererRecorder.h',
  'Renderer/DebugRendererSimple.cpp',
  'Renderer/DebugRendererSimple.h',
  'Skeleton/SkeletalAnimation.cpp',
  'Skeleton/SkeletalAnimation.h',
  'Skeleton/Skeleton.cpp',
  'Skeleton/Skeleton.h',
  'Skeleton/SkeletonMapper.cpp',
  'Skeleton/SkeletonMapper.h',
  'Skeleton/SkeletonPose.cpp',
  'Skeleton/SkeletonPose.h',
  'TriangleSplitter/TriangleSplitter.cpp',
  'TriangleSplitter/TriangleSplitter.h',
  'TriangleSplitter/TriangleSplitterBinning.cpp',
  'TriangleSplitter/TriangleSplitterBinning.h',
  'TriangleSplitter/TriangleSplitterMean.cpp',
  'TriangleSplitter/TriangleSplitterMean.h',
)

inl_sources = files(
  'Physics/Body/MotionProperties.inl',
  'Physics/Body/Body.inl',
  'Math/Vec4.inl',
  'Math/Vec3.inl',
  'Math/UVec4.inl',
  'Math/Quat.inl',
  'Math/Mat44.inl',
  'Math/DVec3.inl',
  'Math/DMat44.inl',
  'Math/BVec16.inl',
  'Core/Profiler.inl',
  'Core/LockFreeHashMap.inl',
  'Core/JobSystem.inl',
  'Core/FixedSizeFreeList.inl',
)

cpp_args = ['-DJPH_SHARED_LIBRARY']
link_args = []

# Setting the disable custom allocator flag
if get_option('disable_custom_allocator')
  cpp_args += '-DJPH_DISABLE_CUSTOM_ALLOCATOR'
endif

# Setting enable asserts flag
if get_option('use_asserts')
	cpp_args += '-DJPH_ENABLE_ASSERTS'
endif

# Setting double precision flag
if get_option('double_precision')
	cpp_args += '-DJPH_DOUBLE_PRECISION'
endif

# Setting to attempt cross platform determinism
if get_option('cross_platform_deterministic')
	cpp_args += '-DJPH_CROSS_PLATFORM_DETERMINISTIC'
endif

# Setting to determine number of bits in ObjectLayer
cpp_args += '-DJPH_OBJECT_LAYER_BITS=@0@'.format(get_option('object_layer_bits'))

if get_option('use_std_vector')
	cpp_args += '-DJPH_USE_STD_VECTOR'
endif

# Setting to periodically trace broadphase stats to help determine if the broadphase layer configuration is optimal
if get_option('track_broadphase_stats')
	cpp_args += '-DJPH_TRACK_BROADPHASE_STATS'
endif

# Setting to periodically trace narrowphase stats to help determine which collision queries could be optimized
if get_option('track_narrowphase_stats')
	cpp_args += '-DJPH_TRACK_NARROWPHASE_STATS'
endif

# Compile the ObjectStream class and RTTI attribute information
if get_option('enable_object_stream')
  cpp_args += '-DJPH_OBJECT_STREAM'
  sources += files(
    'ObjectStream/GetPrimitiveTypeOfType.h',
    'ObjectStream/ObjectStream.cpp',
    'ObjectStream/ObjectStreamBinaryIn.cpp',
    'ObjectStream/ObjectStreamBinaryIn.h',
    'ObjectStream/ObjectStreamBinaryOut.cpp',
    'ObjectStream/ObjectStreamBinaryOut.h',
    'ObjectStream/ObjectStreamIn.cpp',
    'ObjectStream/ObjectStreamIn.h',
    'ObjectStream/ObjectStreamOut.cpp',
    'ObjectStream/ObjectStreamOut.h',
    'ObjectStream/ObjectStreamTextIn.cpp',
    'ObjectStream/ObjectStreamTextIn.h',
    'ObjectStream/ObjectStreamTextOut.cpp',
    'ObjectStream/ObjectStreamTextOut.h',
    'ObjectStream/ObjectStreamTypes.h',
    'ObjectStream/TypeDeclarations.cpp',
  )
endif

x86_simd_definitions = []
if get_option('use_avx512')
    x86_simd_definitions += '-DJPH_USE_AVX512'
endif
if get_option('use_avx2')
    x86_simd_definitions += '-DJPH_USE_AVX2'
endif
if get_option('use_avx')
    x86_simd_definitions += '-DJPH_USE_AVX'
endif
if get_option('use_sse4_1')
    x86_simd_definitions += '-DJPH_USE_SSE4_1'
endif
if get_option('use_sse4_2')
    x86_simd_definitions += '-DJPH_USE_SSE4_2'
endif
if get_option('use_lzcnt')
    x86_simd_definitions += '-DJPH_USE_LZCNT'
endif
if get_option('use_tzcnt')
    x86_simd_definitions += '-DJPH_USE_TZCNT'
endif
if get_option('use_f16c')
    x86_simd_definitions += '-DJPH_USE_F16C'
endif
if get_option('use_fmadd') and not get_option('cross_platform_deterministic')
    x86_simd_definitions += '-DJPH_USE_FMADD'
endif

# compiler specific definitions including simd flags
# I tried to copy this equivalently from upstream jolt, but I'm not testing all this
cpp = meson.get_compiler('cpp')
if cpp.get_id() == 'msvc'
  if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
    if get_option('use_avx512')
      cpp_args += '/arch:AVX512'
    elif get_option('use_avx2')
      cpp_args += '/arch:AVX2'
    elif get_option('use_avx')
      cpp_args += '/arch:AVX'
    endif
    cpp_args += x86_simd_definitions
  endif
elif cpp.get_id() == 'emscripten'
  cpp_args += ['-msimd128', '-msse4.2']
  link_args += ['-sSTACK_SIZE=1048576', '-sINITIAL_MEMORY=134217728', '-Wno-limited-postlink-optimizations']
elif host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
  if get_option('use_avx512')
    cpp_args += ['-mavx512f', '-mavx512vl', '-mavx512dq', '-mavx2', '-mbmi', '-mpopcnt', '-mlzcnt', '-mf16c']
  elif get_option('use_avx2')
    cpp_args += ['-mavx2', '-mbmi', '-mpopcnt', '-mlzcnt', '-mf16c']
  elif get_option('use_avx')
    cpp_args += ['-mavx', '-mpopcnt']
  elif get_option('use_sse4_2')
    cpp_args += ['-msse4.2', '-mpopcnt']
  elif get_option('use_sse4_1')
    cpp_args += ['-msse4.1']
  else
    cpp_args += ['-msse2']
  endif
  if get_option('use_lzcnt')
    cpp_args += '-mlzcnt'
  endif
  if get_option('use_tzcnt')
    cpp_args += '-mbmi'
  endif
  if get_option('use_f16c')
    cpp_args += '-mf16c'
  endif
  if get_option('use_fmadd') and not get_option('cross_platform_deterministic')
    cpp_args += '-mfma'
  endif
  cpp_args += '-mfpmath=sse'
  cpp_args += x86_simd_definitions
endif

jolt_lib = library('Jolt', sources,
  include_directories: includes,
  cpp_args: cpp_args,
  cpp_shared_args: ['-DJPH_SHARED_LIBRARY', '-DJPH_BUILD_SHARED_LIBRARY'],
  extra_files: inl_sources,
  link_args: link_args,
  gnu_symbol_visibility: 'hidden',
  install: get_option('enable_install'),
)
