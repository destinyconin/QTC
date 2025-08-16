#include "zk/Zk.h"
#include <iostream>

#if defined(QTC_ENABLE_ZK) && defined(HAVE_LIBSNARK)
#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp>
using ppT = libsnark::default_r1cs_ppzksnark_pp;
#endif

namespace QTC {

bool Zk::prove_transfer(const std::string& note, std::string& proof) {
#if defined(QTC_ENABLE_ZK) && defined(HAVE_LIBSNARK)
  static bool init = (ppT::init_public_params(), true);
  (void)init;
  auto example = libsnark::generate_r1cs_example_with_field_input<ppT::Fp_type>(100, 10);
  auto keypair = libsnark::r1cs_ppzksnark_generator<ppT>(example.constraint_system);
  auto prf = libsnark::r1cs_ppzksnark_prover<ppT>(keypair.pk, example.primary_input, example.auxiliary_input);
  auto ok = libsnark::r1cs_ppzksnark_verifier_strong_IC<ppT>(keypair.vk, example.primary_input, prf);
  if (!ok) return false;
  proof = "groth16_proof_example";
  (void)note;
  return true;
#else
  (void)note;
  proof = "dummy_proof";
  return true;
#endif
}

bool Zk::verify_transfer(const std::string& proof) {
#if defined(QTC_ENABLE_ZK) && defined(HAVE_LIBSNARK)
  return !proof.empty(); // placeholder
#else
  return proof == "dummy_proof" || proof == "groth16_proof_example";
#endif
}

} // namespace QTC
